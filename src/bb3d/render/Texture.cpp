#include "bb3d/render/Texture.hpp"
#include "bb3d/render/Buffer.hpp"
#include "bb3d/core/Log.hpp"
#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace bb3d {

Texture::Texture(VulkanContext& context, std::string_view filepath)
    : m_context(context) {
    
    stbi_uc* raw_pixels = stbi_load(filepath.data(), &m_width, &m_height, &m_channels, STBI_rgb_alpha);
    
    struct StbiDeleter { void operator()(stbi_uc* p) { stbi_image_free(p); } };
    std::unique_ptr<stbi_uc, StbiDeleter> pixels(raw_pixels);

    if (!pixels) {
        throw std::runtime_error("Failed to load texture image: " + std::string(filepath));
    }

    initFromPixels(pixels.get());
    
    BB_CORE_INFO("Texture chargée: {} ({}x{})", filepath, m_width, m_height);
}

Texture::Texture(VulkanContext& context, std::span<const std::byte> data)
    : m_context(context) {
    
    stbi_uc* raw_pixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(data.data()), static_cast<int>(data.size()), &m_width, &m_height, &m_channels, STBI_rgb_alpha);
    
    struct StbiDeleter { void operator()(stbi_uc* p) { stbi_image_free(p); } };
    std::unique_ptr<stbi_uc, StbiDeleter> pixels(raw_pixels);

    if (!pixels) {
        throw std::runtime_error("Failed to load texture from memory");
    }

    initFromPixels(pixels.get());
    
    BB_CORE_INFO("Texture chargée depuis la mémoire ({}x{})", m_width, m_height);
}

Texture::Texture(VulkanContext& context, std::span<const std::byte> data, int width, int height)
    : m_context(context), m_width(width), m_height(height), m_channels(4) {
    
    if (data.size() != width * height * 4) {
        throw std::runtime_error("Texture: Raw data size mismatch with dimensions");
    }

    initFromPixels(reinterpret_cast<const unsigned char*>(data.data()));
    BB_CORE_INFO("Texture chargée depuis pixels bruts ({}x{})", m_width, m_height);
}

void Texture::initFromPixels(const unsigned char* pixels) {
    vk::DeviceSize imageSize = static_cast<vk::DeviceSize>(m_width) * m_height * 4;

    Buffer stagingBuffer(m_context, imageSize, vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT);
    stagingBuffer.upload(pixels, imageSize);

    createImage(static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height));
    transitionLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    copyBufferToImage(stagingBuffer.getHandle());
    transitionLayout(vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

    createImageView();
    createSampler();
}

Texture::~Texture() {
    auto device = m_context.getDevice();
    if (m_sampler) {
        device.destroySampler(m_sampler);
        BB_CORE_TRACE("Texture: Destroyed sampler for {}", m_path);
    }
    if (m_imageView) {
        device.destroyImageView(m_imageView);
        BB_CORE_TRACE("Texture: Destroyed image view for {}", m_path);
    }
    if (m_image) {
        vmaDestroyImage(m_context.getAllocator(), static_cast<VkImage>(m_image), m_allocation);
        BB_CORE_TRACE("Texture: Destroyed image allocation for {}", m_path);
    }
}

void Texture::createImage(uint32_t width, uint32_t height) {
    vk::ImageCreateInfo imageInfo({}, vk::ImageType::e2D, vk::Format::eR8G8B8A8Srgb, { width, height, 1 }, 1, 1, vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled);

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkImage img;
    vmaCreateImage(m_context.getAllocator(), reinterpret_cast<VkImageCreateInfo*>(&imageInfo), &allocInfo, &img, &m_allocation, nullptr);
    m_image = vk::Image(img);
}

void Texture::createImageView() {
    vk::ImageViewCreateInfo viewInfo({}, m_image, vk::ImageViewType::e2D, vk::Format::eR8G8B8A8Srgb, {}, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
    m_imageView = m_context.getDevice().createImageView(viewInfo);
}

void Texture::createSampler() {
    vk::SamplerCreateInfo samplerInfo({}, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear, vk::SamplerAddressMode::eRepeat, vk::SamplerAddressMode::eRepeat, vk::SamplerAddressMode::eRepeat, 0.0f, VK_FALSE, 1.0f, VK_FALSE, vk::CompareOp::eAlways, 0.0f, 0.0f, vk::BorderColor::eIntOpaqueBlack, VK_FALSE);
    m_sampler = m_context.getDevice().createSampler(samplerInfo);
}

void Texture::transitionLayout(vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
    vk::CommandBuffer commandBuffer = m_context.beginSingleTimeCommands();

    vk::ImageMemoryBarrier barrier({}, {}, oldLayout, newLayout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, m_image, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

    vk::PipelineStageFlags sourceStage;
    vk::PipelineStageFlags destinationStage;

    if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.srcAccessMask = vk::AccessFlags();
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, nullptr, nullptr, barrier);

    m_context.endSingleTimeCommands(commandBuffer);
}

void Texture::copyBufferToImage(vk::Buffer buffer) {
    vk::CommandBuffer commandBuffer = m_context.beginSingleTimeCommands();

    vk::BufferImageCopy region(0, 0, 0, { vk::ImageAspectFlagBits::eColor, 0, 0, 1 }, { 0, 0, 0 }, { static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height), 1 });

    commandBuffer.copyBufferToImage(buffer, m_image, vk::ImageLayout::eTransferDstOptimal, region);

    m_context.endSingleTimeCommands(commandBuffer);
}

} // namespace bb3d
