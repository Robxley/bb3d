#include "bb3d/render/Texture.hpp"
#include "bb3d/render/Buffer.hpp"
#include "bb3d/core/Log.hpp"
#include "bb3d/core/Engine.hpp"
#include <stdexcept>
#include <cmath>

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
    
    BB_CORE_INFO("Texture: Loaded file '{0}' ({1}x{2}, format: {3})", filepath, m_width, m_height, vk::to_string(m_format));
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
    
    BB_CORE_INFO("Texture: Loaded from memory ({0}x{1}, format: {2})", m_width, m_height, vk::to_string(m_format));
}

Texture::Texture(VulkanContext& context, std::span<const std::byte> data, int width, int height, bool isColor)
    : m_context(context), m_width(width), m_height(height), m_channels(4) {
    
    m_format = isColor ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm;

    if (data.size() != (size_t)width * height * 4) {
        throw std::runtime_error("Texture: Raw data size mismatch with dimensions");
    }

    initFromPixels(reinterpret_cast<const unsigned char*>(data.data()));
    BB_CORE_INFO("Texture: Loaded from raw pixels ({0}x{1}, format: {2})", m_width, m_height, vk::to_string(m_format));
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

    m_mipLevels = 1;
    try {
        if (Engine::Get().GetConfig().graphics.enableMipmapping) {
            m_mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(m_width, m_height)))) + 1;
        }
    } catch (...) {}

    m_stagingBuffer = CreateScope<Buffer>(m_context, totalSize, vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT);
    
    for (int i = 0; i < 6; ++i) {
        m_stagingBuffer->upload(all_pixels[i], faceSize, faceSize * i);
        stbi_image_free(all_pixels[i]);
    }

    createImage(m_width, m_height, 6);
    
    vk::CommandBuffer cb = m_context.beginTransferCommands();
    transitionLayout(cb, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, 6);
    copyBufferToImage(cb, m_stagingBuffer->getHandle(), 6);
    
    if (m_mipLevels > 1) {
        generateMipmaps(cb, 6);
    } else {
        transitionLayout(cb, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, 6);
    }

    m_uploadFence = m_context.endTransferCommandsAsync(cb);

    createImageView(6);
    createSampler();

    BB_CORE_INFO("Texture: Loaded cubemap ({0}x{1}, mipLevels: {2})", m_width, m_height, m_mipLevels);
}

Texture::Texture(VulkanContext& context, std::span<const std::byte> data, int width, int height, int layers, bool isColor)
    : m_context(context), m_width(width), m_height(height), m_isCubemap(layers == 6) {
    
    m_format = isColor ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm;

    vk::DeviceSize totalSize = (vk::DeviceSize)width * height * 4 * layers;
    if (data.size() != totalSize) {
        throw std::runtime_error("Texture: Raw data size mismatch with dimensions and layers");
    }

    m_mipLevels = 1;
    try {
        if (Engine::Get().GetConfig().graphics.enableMipmapping) {
            m_mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(m_width, m_height)))) + 1;
        }
    } catch (...) {}

    m_stagingBuffer = CreateScope<Buffer>(m_context, totalSize, vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT);
    m_stagingBuffer->upload(data.data(), totalSize);

    createImage(width, height, layers);
    
    vk::CommandBuffer cb = m_context.beginTransferCommands();
    transitionLayout(cb, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, layers);
    copyBufferToImage(cb, m_stagingBuffer->getHandle(), layers);
    
    if (m_mipLevels > 1) {
        generateMipmaps(cb, layers);
    } else {
        transitionLayout(cb, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, layers);
    }

    m_uploadFence = m_context.endTransferCommandsAsync(cb);

    createImageView(layers);
    createSampler();

    BB_CORE_INFO("Texture: Loaded multi-layer ({0}x{1}, layers: {2}, mipLevels: {3})", width, height, layers, m_mipLevels);
}

void Texture::initFromPixels(const unsigned char* pixels) {
    vk::DeviceSize imageSize = static_cast<vk::DeviceSize>(m_width) * m_height * 4;

    m_mipLevels = 1;
    try {
        if (Engine::Get().GetConfig().graphics.enableMipmapping) {
            m_mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(m_width, m_height)))) + 1;
        }
    } catch (...) {}

    // Conservation du staging buffer jusqu'à la fin de l'upload asynchrone
    m_stagingBuffer = CreateScope<Buffer>(m_context, imageSize, vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT);
    m_stagingBuffer->upload(pixels, imageSize);

    createImage(static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height), 1);

    // Démarrage des commandes de transfert asynchrones
    vk::CommandBuffer cb = m_context.beginTransferCommands();

    transitionLayout(cb, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, 1);
    copyBufferToImage(cb, m_stagingBuffer->getHandle(), 1);
    
    if (m_mipLevels > 1) {
        generateMipmaps(cb, 1);
    } else {
        transitionLayout(cb, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, 1);
    }

    m_uploadFence = m_context.endTransferCommandsAsync(cb);

    createImageView(1);
    createSampler();
    
    // Note: m_ready reste false jusqu'à ce que isReady() ou un wait explicite soit appelé.
}

bool Texture::isReady() {
    if (m_ready) return true;
    if (!m_uploadFence) return true; 

    // Protection contre les appels concurrents (multi-frames)
    auto result = m_context.getDevice().getFenceStatus(m_uploadFence);
    if (result == vk::Result::eSuccess) {
        // Double vérification pour éviter la destruction multiple
        if (m_uploadFence) {
            m_context.getDevice().destroyFence(m_uploadFence);
            m_uploadFence = nullptr;
            m_stagingBuffer.reset();
            m_ready = true;
            BB_CORE_TRACE("Texture: Upload complete and resources released.");
        }
        return true;
    }
    return false;
}

Texture::~Texture() {
    auto device = m_context.getDevice();
    BB_CORE_TRACE("Texture: Destroying texture image ({}x{})", m_width, m_height);
    
    if (m_uploadFence) {
        device.destroyFence(m_uploadFence);
    }

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
    
    vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    if (m_mipLevels > 1) {
        usage |= vk::ImageUsageFlagBits::eTransferSrc; // Needed for blit
    }

    vk::ImageCreateInfo imageInfo(flags, vk::ImageType::e2D, m_format, { width, height, 1 }, m_mipLevels, layers, vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal, usage);

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkImage img;
    vmaCreateImage(m_context.getAllocator(), reinterpret_cast<VkImageCreateInfo*>(&imageInfo), &allocInfo, &img, &m_allocation, nullptr);
    m_image = vk::Image(img);
}

void Texture::createImageView(uint32_t layers) {
    vk::ImageViewType viewType = m_isCubemap ? vk::ImageViewType::eCube : vk::ImageViewType::e2D;
    
    vk::ImageViewCreateInfo viewInfo({}, m_image, viewType, m_format, {}, { vk::ImageAspectFlagBits::eColor, 0, m_mipLevels, 0, layers });
    m_imageView = m_context.getDevice().createImageView(viewInfo);
}

void Texture::createSampler() {
    vk::Filter filter = (m_width <= 256 && m_height <= 256) ? vk::Filter::eNearest : vk::Filter::eLinear;
    vk::SamplerAddressMode addrMode = (m_width <= 256 && m_height <= 256) ? vk::SamplerAddressMode::eClampToEdge : vk::SamplerAddressMode::eRepeat;

    vk::SamplerCreateInfo samplerInfo({}, filter, filter, vk::SamplerMipmapMode::eLinear, addrMode, addrMode, addrMode, 0.0f, VK_FALSE, 1.0f, VK_FALSE, vk::CompareOp::eAlways, 0.0f, static_cast<float>(m_mipLevels), vk::BorderColor::eIntOpaqueBlack, VK_FALSE);
    m_sampler = m_context.getDevice().createSampler(samplerInfo);
}

void Texture::generateMipmaps(vk::CommandBuffer commandBuffer, uint32_t layers) {
    vk::ImageMemoryBarrier barrier({}, {}, vk::ImageLayout::eUndefined, vk::ImageLayout::eUndefined, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, m_image, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, layers });

    int32_t mipWidth = m_width;
    int32_t mipHeight = m_height;

    for (uint32_t i = 1; i < m_mipLevels; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, nullptr, nullptr, barrier);

        vk::ImageBlit blit;
        blit.srcSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, i - 1, 0, layers);
        blit.srcOffsets[0] = vk::Offset3D(0, 0, 0);
        blit.srcOffsets[1] = vk::Offset3D(mipWidth, mipHeight, 1);
        blit.dstSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, i, 0, layers);
        blit.dstOffsets[0] = vk::Offset3D(0, 0, 0);
        blit.dstOffsets[1] = vk::Offset3D(mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1);

        commandBuffer.blitImage(m_image, vk::ImageLayout::eTransferSrcOptimal, m_image, vk::ImageLayout::eTransferDstOptimal, blit, vk::Filter::eLinear);

        barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, nullptr, nullptr, barrier);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = m_mipLevels - 1;
    barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, nullptr, nullptr, barrier);
}

void Texture::transitionLayout(vk::CommandBuffer commandBuffer, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t layers) {
    vk::ImageMemoryBarrier barrier({}, {}, oldLayout, newLayout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, m_image, { vk::ImageAspectFlagBits::eColor, 0, m_mipLevels, 0, layers });

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
}

void Texture::copyBufferToImage(vk::CommandBuffer commandBuffer, vk::Buffer buffer, uint32_t layers) {
    vk::BufferImageCopy region(0, 0, 0, { vk::ImageAspectFlagBits::eColor, 0, 0, layers }, { 0, 0, 0 }, { static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height), 1 });
    commandBuffer.copyBufferToImage(buffer, m_image, vk::ImageLayout::eTransferDstOptimal, region);
}

} // namespace bb3d
