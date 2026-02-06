#include "bb3d/render/SwapChain.hpp"
#include "bb3d/core/Log.hpp"
#include <algorithm>
#include <limits>
#include <array>

namespace bb3d {

SwapChain::SwapChain(VulkanContext& context, int width, int height)
    : m_context(context) {
    createSwapChain(width, height);
    createImageViews();
    createDepthResources();
}

SwapChain::~SwapChain() {
    cleanup();
}

void SwapChain::cleanup() {
    BB_CORE_TRACE("SwapChain: Starting cleanup...");
    auto device = m_context.getDevice();
    if (m_depthImageView) {
        device.destroyImageView(m_depthImageView);
        BB_CORE_TRACE("SwapChain: Destroyed depth image view.");
        m_depthImageView = nullptr;
    }
    if (m_depthImage) {
        vmaDestroyImage(m_context.getAllocator(), static_cast<VkImage>(m_depthImage), m_depthImageAllocation);
        BB_CORE_TRACE("SwapChain: Destroyed depth image.");
        m_depthImage = nullptr;
    }

    for (auto imageView : m_imageViews) {
        device.destroyImageView(imageView);
    }
    BB_CORE_TRACE("SwapChain: Destroyed {} image views.", m_imageViews.size());
    m_imageViews.clear();

    if (m_swapChain) {
        device.destroySwapchainKHR(m_swapChain);
        BB_CORE_TRACE("SwapChain: Destroyed KHR Swapchain.");
        m_swapChain = nullptr;
    }
}

void SwapChain::recreate(int width, int height) {
    if (width == 0 || height == 0) return;
    m_context.getDevice().waitIdle();
    cleanup();
    createSwapChain(width, height);
    createImageViews();
    createDepthResources();
}

uint32_t SwapChain::acquireNextImage(vk::Semaphore semaphore, vk::Fence fence) {
    auto result = m_context.getDevice().acquireNextImageKHR(m_swapChain, std::numeric_limits<uint64_t>::max(), semaphore, fence);

    if (result.result == vk::Result::eErrorOutOfDateKHR) {
        throw std::runtime_error("Swapchain out of date during acquire");
    } else if (result.result != vk::Result::eSuccess && result.result != vk::Result::eSuboptimalKHR) {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    m_currentImageIndex = result.value;
    return m_currentImageIndex;
}

void SwapChain::present(vk::Semaphore waitSemaphore, uint32_t imageIndex) {
    vk::PresentInfoKHR presentInfo(1, &waitSemaphore, 1, &m_swapChain, &imageIndex);

    auto result = m_context.getPresentQueue().presentKHR(presentInfo);

    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR) {
        // Handled by caller
    } else if (result != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to present swap chain image!");
    }
}

void SwapChain::createSwapChain(int width, int height) {
    auto surface = m_context.getSurface();
    auto physicalDevice = m_context.getPhysicalDevice();
    auto device = m_context.getDevice();

    auto capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
    auto formats = physicalDevice.getSurfaceFormatsKHR(surface);
    auto presentModes = physicalDevice.getSurfacePresentModesKHR(surface);

    auto surfaceFormat = chooseSwapSurfaceFormat(formats);
    auto presentMode = chooseSwapPresentMode(presentModes);
    auto extent = chooseSwapExtent(capabilities, width, height);

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR createInfo({}, surface, imageCount, surfaceFormat.format, surfaceFormat.colorSpace, extent, 1, vk::ImageUsageFlagBits::eColorAttachment);

    uint32_t queueFamilyIndices[] = { m_context.getGraphicsQueueFamily(), m_context.getPresentQueueFamily() };

    if (m_context.getGraphicsQueueFamily() != m_context.getPresentQueueFamily()) {
        createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = vk::SharingMode::eExclusive;
    }

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    m_swapChain = device.createSwapchainKHR(createInfo);
    m_images = device.getSwapchainImagesKHR(m_swapChain);
    m_imageFormat = surfaceFormat.format;
    m_extent = extent;
    
    BB_CORE_INFO("Swapchain créée : {}x{} ({})", extent.width, extent.height, m_images.size());
}

void SwapChain::createImageViews() {
    m_imageViews.resize(m_images.size());
    auto device = m_context.getDevice();

    for (size_t i = 0; i < m_images.size(); i++) {
        vk::ImageViewCreateInfo createInfo({}, m_images[i], vk::ImageViewType::e2D, m_imageFormat, {}, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
        m_imageViews[i] = device.createImageView(createInfo);
    }
}

vk::SurfaceFormatKHR SwapChain::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == vk::Format::eB8G8R8A8Srgb && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

vk::PresentModeKHR SwapChain::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
            return availablePresentMode;
        }
    }
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D SwapChain::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, int width, int height) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        vk::Extent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return actualExtent;
    }
}

void SwapChain::createDepthResources() {
    m_depthFormat = findDepthFormat();

    vk::ImageCreateInfo imageInfo({}, vk::ImageType::e2D, m_depthFormat, { m_extent.width, m_extent.height, 1 }, 1, 1, vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment);

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkImage img;
    vmaCreateImage(m_context.getAllocator(), reinterpret_cast<VkImageCreateInfo*>(&imageInfo), &allocInfo, &img, &m_depthImageAllocation, nullptr);
    m_depthImage = vk::Image(img);

    vk::ImageViewCreateInfo viewInfo({}, m_depthImage, vk::ImageViewType::e2D, m_depthFormat, {}, { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 });
    m_depthImageView = m_context.getDevice().createImageView(viewInfo);
}

vk::Format SwapChain::findDepthFormat() {
    std::array<vk::Format, 3> candidates = { vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint };
    for (auto format : candidates) {
        auto props = m_context.getPhysicalDevice().getFormatProperties(format);
        if (props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment) {
            return format;
        }
    }
    throw std::runtime_error("failed to find supported depth format!");
}

} // namespace bb3d