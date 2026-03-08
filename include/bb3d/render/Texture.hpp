#pragma once
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/resource/Resource.hpp"
#include <string_view>
#include <span>

namespace bb3d {

class Texture : public Resource {
public:
    Texture(VulkanContext& context, std::string_view filepath, bool isColor = true);
    Texture(VulkanContext& context, std::span<const std::byte> data, bool isColor = true);
    Texture(VulkanContext& context, std::span<const std::byte> data, int width, int height, bool isColor = true); // Raw RGBA data
    
    /** @brief Constructor for a Cubemap from 6 files. */
    Texture(VulkanContext& context, const std::array<std::string, 6>& filepaths, bool isColor = true);
    Texture(VulkanContext& context, std::span<const std::byte> data, int width, int height, int layers, bool isColor = true); // Raw layered data (e.g. Cubemap)

    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    [[nodiscard]] inline vk::ImageView getImageView() const { return m_imageView; }
    [[nodiscard]] inline vk::Sampler getSampler() const { return m_sampler; }
    [[nodiscard]] inline int getWidth() const { return m_width; }
    [[nodiscard]] inline int getHeight() const { return m_height; }
    [[nodiscard]] inline bool isCubemap() const { return m_isCubemap; }

    /** @brief Checks if GPU upload is complete and texture is ready for use. */
    bool isReady();

private:
    void initFromPixels(const unsigned char* pixels);
    void createImage(uint32_t width, uint32_t height, uint32_t layers = 1);
    void createImageView(uint32_t layers = 1);
    void createSampler();
    
    // Modified methods to support an external CommandBuffer (Async)
    void generateMipmaps(vk::CommandBuffer cb, uint32_t layers = 1);
    void transitionLayout(vk::CommandBuffer cb, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t layers = 1);
    void copyBufferToImage(vk::CommandBuffer cb, vk::Buffer buffer, uint32_t layers = 1);

    VulkanContext& m_context;
    int m_width = 0, m_height = 0, m_channels = 0;
    uint32_t m_mipLevels = 1;
    vk::Format m_format = vk::Format::eR8G8B8A8Srgb;
    bool m_isCubemap = false;
    
    vk::Image m_image;
    VmaAllocation m_allocation = nullptr;
    vk::ImageView m_imageView;
    vk::Sampler m_sampler;

    // Async state
    vk::Fence m_uploadFence = nullptr;
    bool m_ready = false;
    Scope<class Buffer> m_stagingBuffer; // Kept until upload is complete
};

} // namespace bb3d