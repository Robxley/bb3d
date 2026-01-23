#pragma once
#include "bb3d/render/VulkanContext.hpp"
#include <vector>

namespace bb3d {

class SwapChain {
public:
    SwapChain(VulkanContext& context, int width, int height);
    ~SwapChain();

    // Empêcher la copie
    SwapChain(const SwapChain&) = delete;
    SwapChain& operator=(const SwapChain&) = delete;

    // Initialisation et recréation
    void recreate(int width, int height);
    void cleanup();

    // Accesseurs
    [[nodiscard]] VkSwapchainKHR getHandle() const { return m_swapChain; }
    [[nodiscard]] VkFormat getImageFormat() const { return m_imageFormat; }
    [[nodiscard]] VkExtent2D getExtent() const { return m_extent; }
    [[nodiscard]] const std::vector<VkImageView>& getImageViews() const { return m_imageViews; }
    [[nodiscard]] size_t getImageCount() const { return m_images.size(); }

    // Opérations de Frame
    // Retourne l'index de l'image acquise
    uint32_t acquireNextImage(VkSemaphore semaphore, VkFence fence = VK_NULL_HANDLE);
    void present(VkSemaphore waitSemaphore, uint32_t imageIndex);

private:
    void createSwapChain(int width, int height);
    void createImageViews();
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, int width, int height);

    VulkanContext& m_context;
    VkSwapchainKHR m_swapChain{VK_NULL_HANDLE};
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;
    VkFormat m_imageFormat;
    VkExtent2D m_extent;
};

}
