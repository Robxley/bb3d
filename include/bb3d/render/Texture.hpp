#pragma once
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/resource/Resource.hpp" // Added
#include <string_view>

namespace bb3d {

class Texture : public Resource { // Inherit
public:
    Texture(VulkanContext& context, std::string_view filepath);
    Texture(VulkanContext& context, const void* data, size_t size); // Constructeur depuis mémoire
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    [[nodiscard]] VkImageView getImageView() const { return m_imageView; }
    [[nodiscard]] VkSampler getSampler() const { return m_sampler; }
    [[nodiscard]] int getWidth() const { return m_width; }
    [[nodiscard]] int getHeight() const { return m_height; }

private:
    void createImage(uint32_t width, uint32_t height);
    void createImageView();
    void createSampler();
    void transitionLayout(VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer);

    VulkanContext& m_context;
    int m_width{0}, m_height{0}, m_channels{0};
    
    VkImage m_image{VK_NULL_HANDLE};
    VmaAllocation m_allocation{VK_NULL_HANDLE};
    VkImageView m_imageView{VK_NULL_HANDLE};
    VkSampler m_sampler{VK_NULL_HANDLE};
};

} // namespace bb3d
