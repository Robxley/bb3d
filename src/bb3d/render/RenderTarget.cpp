#include "bb3d/render/RenderTarget.hpp"
#include <stdexcept>

namespace bb3d {

static vk::Format findSupportedFormat(vk::PhysicalDevice physicalDevice, const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) {
    for (vk::Format format : candidates) {
        vk::FormatProperties props = physicalDevice.getFormatProperties(format);
        if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features) return format;
        if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features) return format;
    }
    throw std::runtime_error("failed to find supported format!");
}

RenderTarget::RenderTarget(VulkanContext& context, uint32_t width, uint32_t height)
    : m_context(context), m_width(width), m_height(height) {
    
    // Trouver le format de profondeur
    m_depthFormat = findSupportedFormat(
        m_context.getPhysicalDevice(),
        {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
        vk::ImageTiling::eOptimal,
        vk::FormatFeatureFlagBits::eDepthStencilAttachment
    );

    // Créer le sampler (une fois pour toutes, pas besoin de le recréer au resize)
    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter = vk::Filter::eLinear;
    samplerInfo.minFilter = vk::Filter::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.anisotropyEnable = VK_FALSE; // Pas besoin pour une render target 1:1
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = vk::BorderColor::eFloatOpaqueWhite;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    
    m_sampler = m_context.getDevice().createSampler(samplerInfo);

    createImages();
}

RenderTarget::~RenderTarget() {
    cleanup();
    if (m_sampler) m_context.getDevice().destroySampler(m_sampler);
}

void RenderTarget::resize(uint32_t width, uint32_t height) {
    if (width == m_width && height == m_height) return;
    m_width = width;
    m_height = height;
    
    // Attendre que le GPU ait fini avant de détruire
    m_context.getDevice().waitIdle(); 
    cleanup();
    createImages();
}

void RenderTarget::cleanup() {
    auto dev = m_context.getDevice();
    if (m_colorImageView) { dev.destroyImageView(m_colorImageView); m_colorImageView = nullptr; }
    if (m_colorImage) { vmaDestroyImage(m_context.getAllocator(), m_colorImage, m_colorAllocation); m_colorImage = nullptr; }
    
    if (m_depthImageView) { dev.destroyImageView(m_depthImageView); m_depthImageView = nullptr; }
    if (m_depthImage) { vmaDestroyImage(m_context.getAllocator(), m_depthImage, m_depthAllocation); m_depthImage = nullptr; }
}

void RenderTarget::createImages() {
    auto allocator = m_context.getAllocator();
    auto dev = m_context.getDevice();

    // --- 1. Color Image ---
    vk::ImageCreateInfo colorInfo{};
    colorInfo.imageType = vk::ImageType::e2D;
    colorInfo.extent.width = m_width;
    colorInfo.extent.height = m_height;
    colorInfo.extent.depth = 1;
    colorInfo.mipLevels = 1;
    colorInfo.arrayLayers = 1;
    colorInfo.format = m_colorFormat;
    colorInfo.tiling = vk::ImageTiling::eOptimal;
    colorInfo.initialLayout = vk::ImageLayout::eUndefined;
    colorInfo.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
    colorInfo.samples = vk::SampleCountFlagBits::e1;
    colorInfo.sharingMode = vk::SharingMode::eExclusive;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE; // GPU Only

    VkImage cImg;
    vmaCreateImage(allocator, reinterpret_cast<const VkImageCreateInfo*>(&colorInfo), &allocInfo, &cImg, &m_colorAllocation, nullptr);
    m_colorImage = cImg;

    // ImageView
    vk::ImageViewCreateInfo colorViewInfo{};
    colorViewInfo.image = m_colorImage;
    colorViewInfo.viewType = vk::ImageViewType::e2D;
    colorViewInfo.format = m_colorFormat;
    colorViewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    colorViewInfo.subresourceRange.baseMipLevel = 0;
    colorViewInfo.subresourceRange.levelCount = 1;
    colorViewInfo.subresourceRange.baseArrayLayer = 0;
    colorViewInfo.subresourceRange.layerCount = 1;
    m_colorImageView = dev.createImageView(colorViewInfo);

    // --- 2. Depth Image ---
    vk::ImageCreateInfo depthInfo = colorInfo;
    depthInfo.format = m_depthFormat;
    depthInfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment; // Pas besoin de sample le depth pour l'instant

    VkImage dImg;
    vmaCreateImage(allocator, reinterpret_cast<const VkImageCreateInfo*>(&depthInfo), &allocInfo, &dImg, &m_depthAllocation, nullptr);
    m_depthImage = dImg;

    // ImageView
    vk::ImageViewCreateInfo depthViewInfo = colorViewInfo;
    depthViewInfo.image = m_depthImage;
    depthViewInfo.format = m_depthFormat;
    depthViewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
    m_depthImageView = dev.createImageView(depthViewInfo);
}

} // namespace bb3d
