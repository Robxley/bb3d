#pragma once
#include "bb3d/render/VulkanContext.hpp"
#include <vector>

namespace bb3d {

class SwapChain {
public:
    SwapChain(VulkanContext& context, int width, int height);
    ~SwapChain();

    SwapChain(const SwapChain&) = delete;
    SwapChain& operator=(const SwapChain&) = delete;

    void recreate(int width, int height);
    void cleanup();

    [[nodiscard]] inline vk::SwapchainKHR getHandle() const { return m_swapChain; }
    [[nodiscard]] inline vk::Format getImageFormat() const { return m_imageFormat; }
    [[nodiscard]] inline vk::Extent2D getExtent() const { return m_extent; }
    [[nodiscard]] inline const std::vector<vk::ImageView>& getImageViews() const { return m_imageViews; }
    [[nodiscard]] inline vk::Image getImage(uint32_t index) const { return m_images[index]; }
    [[nodiscard]] inline size_t getImageCount() const { return m_images.size(); }
    
    [[nodiscard]] inline vk::Image getDepthImage() const { return m_depthImage; }
    [[nodiscard]] inline vk::ImageView getDepthImageView() const { return m_depthImageView; }
    [[nodiscard]] inline vk::Format getDepthFormat() const { return m_depthFormat; }

    uint32_t acquireNextImage(vk::Semaphore semaphore, vk::Fence fence = nullptr);
    void present(vk::Semaphore waitSemaphore, uint32_t imageIndex);

private:
    void createSwapChain(int width, int height);
    void createImageViews();
    void createDepthResources();
    vk::Format findDepthFormat();

    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes);
    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, int width, int height);

    VulkanContext& m_context;
    vk::SwapchainKHR m_swapChain;
    std::vector<vk::Image> m_images;
    std::vector<vk::ImageView> m_imageViews;
    vk::Format m_imageFormat;
    vk::Extent2D m_extent;

    vk::Image m_depthImage;
    VmaAllocation m_depthImageAllocation = nullptr;
    vk::ImageView m_depthImageView;
    vk::Format m_depthFormat = vk::Format::eUndefined;
};

} // namespace bb3d