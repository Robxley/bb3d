#pragma once
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/resource/Resource.hpp"
#include <string_view>
#include <span>

namespace bb3d {

class Texture : public Resource {
public:
    Texture(VulkanContext& context, std::string_view filepath);
    Texture(VulkanContext& context, std::span<const std::byte> data);
    Texture(VulkanContext& context, std::span<const std::byte> data, int width, int height); // Raw RGBA data
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    [[nodiscard]] inline vk::ImageView getImageView() const { return m_imageView; }
    [[nodiscard]] inline vk::Sampler getSampler() const { return m_sampler; }
    [[nodiscard]] inline int getWidth() const { return m_width; }
    [[nodiscard]] inline int getHeight() const { return m_height; }

private:
    void initFromPixels(const unsigned char* pixels);
    void createImage(uint32_t width, uint32_t height);
    void createImageView();
    void createSampler();
    void transitionLayout(vk::ImageLayout oldLayout, vk::ImageLayout newLayout);
    void copyBufferToImage(vk::Buffer buffer);

    VulkanContext& m_context;
    int m_width = 0, m_height = 0, m_channels = 0;
    
    vk::Image m_image;
    VmaAllocation m_allocation = nullptr;
    vk::ImageView m_imageView;
    vk::Sampler m_sampler;
};

} // namespace bb3d