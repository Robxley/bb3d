#pragma once

#include "bb3d/core/Base.hpp"
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>
#include <string>
#include <string_view>

struct SDL_Window;

namespace bb3d {

/**
 * @brief Gère l'initialisation et la destruction du contexte Vulkan de bas niveau via Vulkan-Hpp.
 * (Instance, Surface, PhysicalDevice, Device, VMA).
 */
class VulkanContext {
public:
    VulkanContext();
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    void init(SDL_Window* window, std::string_view appName, bool enableValidationLayers);
    void cleanup();

    [[nodiscard]] inline vk::Instance getInstance() const { return m_instance; }
    [[nodiscard]] inline vk::SurfaceKHR getSurface() const { return m_surface; }
    [[nodiscard]] inline vk::PhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
    [[nodiscard]] inline vk::Device getDevice() const { return m_device; }
    [[nodiscard]] inline vk::Queue getGraphicsQueue() const { return m_graphicsQueue; }
    [[nodiscard]] inline vk::Queue getPresentQueue() const { return m_presentQueue; }
    [[nodiscard]] inline uint32_t getGraphicsQueueFamily() const { return m_graphicsQueueFamily; }
    [[nodiscard]] inline uint32_t getPresentQueueFamily() const { return m_presentQueueFamily; }
    [[nodiscard]] inline VmaAllocator getAllocator() const { return m_allocator; }
    [[nodiscard]] inline std::string_view getDeviceName() const { return m_deviceName; }

    // Helpers pour commandes uniques
    vk::CommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(vk::CommandBuffer commandBuffer);

private:
    vk::Instance m_instance;
    vk::DebugUtilsMessengerEXT m_debugMessenger;
    vk::SurfaceKHR m_surface;
    vk::PhysicalDevice m_physicalDevice;
    vk::Device m_device;
    
    vk::Queue m_graphicsQueue;
    vk::Queue m_presentQueue;
    uint32_t m_graphicsQueueFamily = 0;
    uint32_t m_presentQueueFamily = 0;

    VmaAllocator m_allocator = nullptr;
    vk::CommandPool m_shortLivedCommandPool;
    std::string m_deviceName;
};

} // namespace bb3d