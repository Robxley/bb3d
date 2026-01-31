#include "bb3d/render/Texture.hpp"
#include "bb3d/render/Buffer.hpp"
#include "bb3d/core/Log.hpp"
#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace bb3d {

Texture::Texture(VulkanContext& context, std::string_view filepath, bool isColor)
    : m_context(context) {
    
    m_format = isColor ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm;

    stbi_uc* raw_pixels = stbi_load(filepath.data(), &m_width, &m_height, &m_channels, STBI_rgb_alpha);
    
    struct StbiDeleter { void operator()(stbi_uc* p) { stbi_image_free(p); } };
    std::unique_ptr<stbi_uc, StbiDeleter> pixels(raw_pixels);

    if (!pixels) {
        throw std::runtime_error("Failed to load texture image: " + std::string(filepath));
    }

    initFromPixels(pixels.get());
    
    BB_CORE_INFO("Texture chargée: {} ({}x{}, format: {})", filepath, m_width, m_height, vk::to_string(m_format));
}

Texture::Texture(VulkanContext& context, std::span<const std::byte> data, bool isColor)
    : m_context(context) {
    
    m_format = isColor ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm;

    stbi_uc* raw_pixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(data.data()), static_cast<int>(data.size()), &m_width, &m_height, &m_channels, STBI_rgb_alpha);
    
    struct StbiDeleter { void operator()(stbi_uc* p) { stbi_image_free(p); } };
    std::unique_ptr<stbi_uc, StbiDeleter> pixels(raw_pixels);

    if (!pixels) {
        throw std::runtime_error("Failed to load texture from memory");
    }

    initFromPixels(pixels.get());
    
    BB_CORE_INFO("Texture chargée depuis la mémoire ({}x{}, format: {})", m_width, m_height, vk::to_string(m_format));
}

Texture::Texture(VulkanContext& context, std::span<const std::byte> data, int width, int height, bool isColor)
    : m_context(context), m_width(width), m_height(height), m_channels(4) {
    
    m_format = isColor ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm;

    if (data.size() != (size_t)width * height * 4) {
        throw std::runtime_error("Texture: Raw data size mismatch with dimensions");
    }

    initFromPixels(reinterpret_cast<const unsigned char*>(data.data()));
    BB_CORE_INFO("Texture chargée depuis pixels bruts ({}x{}, format: {})", m_width, m_height, vk::to_string(m_format));
}

Texture::Texture(VulkanContext& context, const std::array<std::string, 6>& filepaths, bool isColor)
    : m_context(context), m_isCubemap(true) {
    
    m_format = isColor ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm;

    std::array<stbi_uc*, 6> all_pixels;
    int w, h, ch;

    for (int i = 0; i < 6; ++i) {
        all_pixels[i] = stbi_load(filepaths[i].c_str(), &w, &h, &ch, STBI_rgb_alpha);
        if (!all_pixels[i]) {
            for (int j = 0; j < i; ++j) stbi_image_free(all_pixels[j]);
            throw std::runtime_error("Failed to load cubemap face: " + filepaths[i]);
        }
        if (i > 0 && (w != m_width || h != m_height)) {
            for (int j = 0; j <= i; ++j) stbi_image_free(all_pixels[j]);
            throw std::runtime_error("Cubemap faces must have same dimensions");
        }
        m_width = w; m_height = h;
    }

    vk::DeviceSize faceSize = (vk::DeviceSize)m_width * m_height * 4;
    vk::DeviceSize totalSize = faceSize * 6;

    Buffer stagingBuffer(m_context, totalSize, vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT);
    
    for (int i = 0; i < 6; ++i) {
        stagingBuffer.upload(all_pixels[i], faceSize, faceSize * i);
        stbi_image_free(all_pixels[i]);
    }

    createImage(m_width, m_height, 6);
    transitionLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, 6);
    copyBufferToImage(stagingBuffer.getHandle(), 6);
    transitionLayout(vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, 6);

    createImageView(6);
    createSampler();

    BB_CORE_INFO("Cubemap chargée: {}x{}", m_width, m_height);
}

Texture::Texture(VulkanContext& context, std::span<const std::byte> data, int width, int height, int layers, bool isColor)
    : m_context(context), m_width(width), m_height(height), m_isCubemap(layers == 6) {
    
    m_format = isColor ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm;

    vk::DeviceSize totalSize = (vk::DeviceSize)width * height * 4 * layers;
    if (data.size() != totalSize) {
        throw std::runtime_error("Texture: Raw data size mismatch with dimensions and layers");
    }

    Buffer stagingBuffer(m_context, totalSize, vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT);
    stagingBuffer.upload(data.data(), totalSize);

    createImage(width, height, layers);
    transitionLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, layers);
    copyBufferToImage(stagingBuffer.getHandle(), layers);
    transitionLayout(vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, layers);

    createImageView(layers);
    createSampler();

    BB_CORE_INFO("Texture multicouche chargée ({}x{}, layers: {})", width, height, layers);
}

void Texture::initFromPixels(const unsigned char* pixels) {
    vk::DeviceSize imageSize = static_cast<vk::DeviceSize>(m_width) * m_height * 4;

    Buffer stagingBuffer(m_context, imageSize, vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT);
    stagingBuffer.upload(pixels, imageSize);

    createImage(static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height), 1);
    transitionLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, 1);
    copyBufferToImage(stagingBuffer.getHandle(), 1);
    transitionLayout(vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, 1);

    createImageView(1);
    createSampler();
}

Texture::~Texture() {
    auto device = m_context.getDevice();
    if (m_sampler) {
        device.destroySampler(m_sampler);
    }
    if (m_imageView) {
        device.destroyImageView(m_imageView);
    }
    if (m_image) {
        vmaDestroyImage(m_context.getAllocator(), static_cast<VkImage>(m_image), m_allocation);
    }
}

void Texture::createImage(uint32_t width, uint32_t height, uint32_t layers) {
    vk::ImageCreateFlags flags = m_isCubemap ? vk::ImageCreateFlagBits::eCubeCompatible : vk::ImageCreateFlags();
    
    vk::ImageCreateInfo imageInfo(flags, vk::ImageType::e2D, m_format, { width, height, 1 }, 1, layers, vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled);

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkImage img;
    vmaCreateImage(m_context.getAllocator(), reinterpret_cast<VkImageCreateInfo*>(&imageInfo), &allocInfo, &img, &m_allocation, nullptr);
    m_image = vk::Image(img);
}

void Texture::createImageView(uint32_t layers) {
    vk::ImageViewType viewType = m_isCubemap ? vk::ImageViewType::eCube : vk::ImageViewType::e2D;
    
    vk::ImageViewCreateInfo viewInfo({}, m_image, viewType, m_format, {}, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, layers });
    m_imageView = m_context.getDevice().createImageView(viewInfo);
}

void Texture::createSampler() {
    vk::SamplerCreateInfo samplerInfo({}, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear, vk::SamplerAddressMode::eRepeat, vk::SamplerAddressMode::eRepeat, vk::SamplerAddressMode::eRepeat, 0.0f, VK_FALSE, 1.0f, VK_FALSE, vk::CompareOp::eAlways, 0.0f, 0.0f, vk::BorderColor::eIntOpaqueBlack, VK_FALSE);
    m_sampler = m_context.getDevice().createSampler(samplerInfo);
}

void Texture::transitionLayout(vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t layers) {
    vk::CommandBuffer commandBuffer = m_context.beginSingleTimeCommands();

    vk::ImageMemoryBarrier barrier({}, {}, oldLayout, newLayout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, m_image, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, layers });

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

void Texture::copyBufferToImage(vk::Buffer buffer, uint32_t layers) {
    vk::CommandBuffer commandBuffer = m_context.beginSingleTimeCommands();

    vk::BufferImageCopy region(0, 0, 0, { vk::ImageAspectFlagBits::eColor, 0, 0, layers }, { 0, 0, 0 }, { static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height), 1 });

    commandBuffer.copyBufferToImage(buffer, m_image, vk::ImageLayout::eTransferDstOptimal, region);

    m_context.endSingleTimeCommands(commandBuffer);
}

} // namespace bb3d
