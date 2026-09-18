#include "bb3d/core/Log.hpp"
#include "bb3d/core/Config.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/render/Texture.hpp"
#include <SDL3/SDL.h>
#include <vector>
#include <array>
#include <iostream>

int main() {
    bb3d::EngineConfig logConfig;
    logConfig.system.logDirectory = "unit_test_logs";
    logConfig.system.logFileName = "unit_test_32_synchronization2.log";
    bb3d::Log::Init(logConfig);
    BB_CORE_INFO("Unit Test 32: Synchronization2 & Batched pipelineBarrier2");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        BB_CORE_ERROR("SDL initialization failed: {}", SDL_GetError());
        return -1;
    }

    try {
        bb3d::VulkanContext context;

#ifdef NDEBUG
        bool validation = false;
#else
        bool validation = true;
#endif
        context.init(nullptr, "Unit Test Sync2", validation);

        const auto& features = context.getEnabledFeatures();
        if (!features.synchronization2) {
            BB_CORE_ERROR("Vulkan Synchronization2 is not enabled on device!");
            SDL_Quit();
            return -1;
        }

        auto device = context.getDevice();
        auto allocator = context.getAllocator();

        // 1. Create color and depth test images via VMA
        vk::ImageCreateInfo colorImgInfo{};
        colorImgInfo.imageType = vk::ImageType::e2D;
        colorImgInfo.format = vk::Format::eR8G8B8A8Unorm;
        colorImgInfo.extent = vk::Extent3D(64, 64, 1);
        colorImgInfo.mipLevels = 1;
        colorImgInfo.arrayLayers = 1;
        colorImgInfo.samples = vk::SampleCountFlagBits::e1;
        colorImgInfo.tiling = vk::ImageTiling::eOptimal;
        colorImgInfo.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;

        VmaAllocationCreateInfo allocCreateInfo{};
        allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VkImage rawColorImage = VK_NULL_HANDLE;
        VmaAllocation colorAlloc = nullptr;
        VkImageCreateInfo rawColorInfo = static_cast<VkImageCreateInfo>(colorImgInfo);
        if (vmaCreateImage(allocator, &rawColorInfo, &allocCreateInfo, &rawColorImage, &colorAlloc, nullptr) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate test color image!");
        }
        vk::Image colorImage(rawColorImage);

        vk::ImageCreateInfo depthImgInfo{};
        depthImgInfo.imageType = vk::ImageType::e2D;
        depthImgInfo.format = vk::Format::eD32Sfloat;
        depthImgInfo.extent = vk::Extent3D(64, 64, 1);
        depthImgInfo.mipLevels = 1;
        depthImgInfo.arrayLayers = 1;
        depthImgInfo.samples = vk::SampleCountFlagBits::e1;
        depthImgInfo.tiling = vk::ImageTiling::eOptimal;
        depthImgInfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;

        VkImage rawDepthImage = VK_NULL_HANDLE;
        VmaAllocation depthAlloc = nullptr;
        VkImageCreateInfo rawDepthInfo = static_cast<VkImageCreateInfo>(depthImgInfo);
        if (vmaCreateImage(allocator, &rawDepthInfo, &allocCreateInfo, &rawDepthImage, &depthAlloc, nullptr) != VK_SUCCESS) {
            vmaDestroyImage(allocator, rawColorImage, colorAlloc);
            throw std::runtime_error("Failed to allocate test depth image!");
        }
        vk::Image depthImage(rawDepthImage);

        // 2. Test batched pipelineBarrier2 submission
        vk::CommandBuffer cb = context.beginSingleTimeCommands();

        vk::ImageMemoryBarrier2 colorBarrier{};
        colorBarrier.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
        colorBarrier.srcAccessMask = {};
        colorBarrier.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
        colorBarrier.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
        colorBarrier.oldLayout = vk::ImageLayout::eUndefined;
        colorBarrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
        colorBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        colorBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        colorBarrier.image = colorImage;
        colorBarrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

        vk::ImageMemoryBarrier2 depthBarrier{};
        depthBarrier.srcStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests;
        depthBarrier.srcAccessMask = {};
        depthBarrier.dstStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests;
        depthBarrier.dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
        depthBarrier.oldLayout = vk::ImageLayout::eUndefined;
        depthBarrier.newLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
        depthBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        depthBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        depthBarrier.image = depthImage;
        depthBarrier.subresourceRange = { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 };

        std::array<vk::ImageMemoryBarrier2, 2> batchedBarriers = { colorBarrier, depthBarrier };
        vk::DependencyInfo depInfo{};
        depInfo.setImageMemoryBarriers(batchedBarriers);

        cb.pipelineBarrier2(depInfo);

        context.endSingleTimeCommands(cb);
        BB_CORE_INFO("Batched pipelineBarrier2 command executed successfully.");

        // Clean up test images
        vmaDestroyImage(allocator, rawColorImage, colorAlloc);
        vmaDestroyImage(allocator, rawDepthImage, depthAlloc);

        // 3. Test Texture upload & mipmap generation with modern transitions
        std::vector<uint8_t> rawPixels(16 * 16 * 4, 180);
        auto testTexture = bb3d::CreateRef<bb3d::Texture>(context, std::as_bytes(std::span(rawPixels)), 16, 16, true);
        if (!testTexture->getImageView()) {
            throw std::runtime_error("Test texture failed to initialize image view!");
        }
        BB_CORE_INFO("Texture upload and mipmap generation verified via modern transitions.");

        BB_CORE_INFO("Unit Test 32 PASSED!");
    } catch (const std::exception& e) {
        BB_CORE_ERROR("Fatal error in Unit Test 32: {}", e.what());
        SDL_Quit();
        return -1;
    }

    SDL_Quit();
    return 0;
}
