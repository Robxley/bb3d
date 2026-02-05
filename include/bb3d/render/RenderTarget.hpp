#pragma once

#include "bb3d/core/Base.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/render/Texture.hpp" // Pour réutiliser potentiellement des helpers ou types

namespace bb3d {

class RenderTarget {
public:
    RenderTarget(VulkanContext& context, uint32_t width, uint32_t height);
    ~RenderTarget();

    void resize(uint32_t width, uint32_t height);

    [[nodiscard]] vk::Image getColorImage() const { return m_colorImage; }
    [[nodiscard]] vk::ImageView getColorImageView() const { return m_colorImageView; }
    [[nodiscard]] vk::Format getColorFormat() const { return m_colorFormat; }
    
    [[nodiscard]] vk::Image getDepthImage() const { return m_depthImage; }
    [[nodiscard]] vk::ImageView getDepthImageView() const { return m_depthImageView; }
    [[nodiscard]] vk::Format getDepthFormat() const { return m_depthFormat; }

    [[nodiscard]] vk::Sampler getSampler() const { return m_sampler; }
    [[nodiscard]] vk::Extent2D getExtent() const { return {m_width, m_height}; }

private:
    void createImages();
    void cleanup();

    VulkanContext& m_context;
    uint32_t m_width;
    uint32_t m_height;

    // Color Attachment (HDR Float)
    vk::Image m_colorImage;
    VmaAllocation m_colorAllocation = nullptr;
    vk::ImageView m_colorImageView;
    vk::Format m_colorFormat = vk::Format::eR16G16B16A16Sfloat;

    // Depth Attachment
    vk::Image m_depthImage;
    VmaAllocation m_depthAllocation = nullptr;
    vk::ImageView m_depthImageView;
    vk::Format m_depthFormat;

    // Sampler pour lire la couleur
    vk::Sampler m_sampler;
};

} // namespace bb3d
