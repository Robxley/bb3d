#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <string>
#include <string_view>

struct SDL_Window;

namespace bb3d {

/**
 * @brief Gère l'initialisation et la destruction du contexte Vulkan de bas niveau.
 * (Instance, Surface, PhysicalDevice, Device, VMA).
 */
class VulkanContext {
public:
    VulkanContext() = default;
    ~VulkanContext();

    // Empêcher la copie
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    void init(SDL_Window* window, std::string_view appName, bool enableValidationLayers);
    void cleanup();

    [[nodiscard]] VkInstance getInstance() const { return m_instance; }
    [[nodiscard]] VkSurfaceKHR getSurface() const { return m_surface; }
    [[nodiscard]] VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
    [[nodiscard]] VkDevice getDevice() const { return m_device; }
    [[nodiscard]] VkQueue getGraphicsQueue() const { return m_graphicsQueue; }
    [[nodiscard]] VkQueue getPresentQueue() const { return m_presentQueue; }
    [[nodiscard]] uint32_t getGraphicsQueueFamily() const { return m_graphicsQueueFamily; }
    [[nodiscard]] uint32_t getPresentQueueFamily() const { return m_presentQueueFamily; }
    [[nodiscard]] VmaAllocator getAllocator() const { return m_allocator; }
    [[nodiscard]] std::string getDeviceName() const { return m_deviceName; }

    // Helpers pour commandes uniques (Transferts, Transitions)
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

private:
    VkInstance m_instance{VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT m_debugMessenger{VK_NULL_HANDLE};
    VkSurfaceKHR m_surface{VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
    VkDevice m_device{VK_NULL_HANDLE};
    VkQueue m_graphicsQueue{VK_NULL_HANDLE};
    VkQueue m_presentQueue{VK_NULL_HANDLE};
    uint32_t m_graphicsQueueFamily{0};
    uint32_t m_presentQueueFamily{0};
    VmaAllocator m_allocator{VK_NULL_HANDLE};
    VkCommandPool m_shortLivedCommandPool{VK_NULL_HANDLE}; // Pool persistant pour commandes courtes
    std::string m_deviceName;
};

} // namespace bb3d