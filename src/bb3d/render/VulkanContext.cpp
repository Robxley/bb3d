#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/core/Log.hpp"

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <set>

namespace bb3d {

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    
    (void)messageType;
    (void)pUserData;

    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        BB_CORE_ERROR("Validation Layer: {}", pCallbackData->pMessage);
    } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        BB_CORE_WARN("Validation Layer: {}", pCallbackData->pMessage);
    }
    
    return VK_FALSE;
}

VulkanContext::VulkanContext() = default;

VulkanContext::~VulkanContext() {
    cleanup();
}

void VulkanContext::init(SDL_Window* window, std::string_view appName, bool enableValidationLayers) {
    // Initialisation du Dispatch Dynamique pour les fonctions de base (vkGetInstanceProcAddr)
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

    // 1. Instance
    vk::ApplicationInfo appInfo(appName.data(), VK_MAKE_VERSION(1, 0, 0), "biobazard3d", VK_MAKE_VERSION(1, 0, 0), VK_API_VERSION_1_3);

    uint32_t sdlExtensionCount = 0;
    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
    std::vector<const char*> extensions(sdlExtensions, sdlExtensions + sdlExtensionCount);

    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    vk::InstanceCreateInfo createInfo({}, &appInfo, 0, nullptr, static_cast<uint32_t>(extensions.size()), extensions.data());

    const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }

    m_instance = vk::createInstance(createInfo);
    
    // Initialisation du Dispatch Dynamique pour les extensions de l'instance
    VULKAN_HPP_DEFAULT_DISPATCHER.init(m_instance);

    BB_CORE_INFO("VulkanContext: Instance Vulkan créée (Vulkan-Hpp).");

    // 2. Debug Messenger
    if (enableValidationLayers) {
        vk::DebugUtilsMessengerCreateInfoEXT debugInfo(
            {},
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
            reinterpret_cast<PFN_vkDebugUtilsMessengerCallbackEXT>(debugCallback)
        );
        m_debugMessenger = m_instance.createDebugUtilsMessengerEXT(debugInfo);
        BB_CORE_INFO("VulkanContext: Debug Messenger activé.");
    }

    // 3. Surface
    if (window) {
        VkSurfaceKHR surface;
        if (!SDL_Vulkan_CreateSurface(window, static_cast<VkInstance>(m_instance), nullptr, &surface)) {
            throw std::runtime_error("Failed to create SDL Vulkan Surface");
        }
        m_surface = vk::SurfaceKHR(surface);
    }

    // 4. Physical Device
    auto physicalDevices = m_instance.enumeratePhysicalDevices();
    if (physicalDevices.empty()) {
        throw std::runtime_error("No Vulkan GPUs found");
    }

    for (const auto& device : physicalDevices) {
        auto props = device.getProperties();
        auto queueFamilies = device.getQueueFamilyProperties();

        int graphicsIndex = -1;
        int presentIndex = -1;

        for (uint32_t i = 0; i < queueFamilies.size(); ++i) {
            if (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) {
                graphicsIndex = i;
            }
            
            if (m_surface) {
                if (device.getSurfaceSupportKHR(i, m_surface)) {
                    presentIndex = i;
                }
            } else {
                presentIndex = graphicsIndex; // If no surface, just use graphics index
            }
            
            if (graphicsIndex != -1 && presentIndex != -1) break;
        }

        if (graphicsIndex != -1 && presentIndex != -1) {
            m_physicalDevice = device;
            m_graphicsQueueFamily = graphicsIndex;
            m_presentQueueFamily = presentIndex;
            m_deviceName = props.deviceName.data();
            if (props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) break;
        }
    }

    BB_CORE_INFO("VulkanContext: GPU sélectionné : {}", m_deviceName);

    // 5. Logical Device
    std::set<uint32_t> uniqueQueueFamilies = { m_graphicsQueueFamily, m_presentQueueFamily };
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    float queuePriority = 1.0f;
    for (uint32_t family : uniqueQueueFamilies) {
        queueCreateInfos.push_back({ {}, family, 1, &queuePriority });
    }

    std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    
    vk::PhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures(VK_TRUE);
    vk::PhysicalDeviceFeatures deviceFeatures{}; // Basic features
    
    vk::DeviceCreateInfo deviceCreateInfo({}, 
        static_cast<uint32_t>(queueCreateInfos.size()), queueCreateInfos.data(),
        0, nullptr,
        static_cast<uint32_t>(deviceExtensions.size()), deviceExtensions.data(),
        &deviceFeatures);
    
    deviceCreateInfo.pNext = &dynamicRenderingFeatures;

    m_device = m_physicalDevice.createDevice(deviceCreateInfo);
    
    // Initialisation du Dispatch Dynamique pour les extensions du device
    VULKAN_HPP_DEFAULT_DISPATCHER.init(m_device);

    m_graphicsQueue = m_device.getQueue(m_graphicsQueueFamily, 0);
    m_presentQueue = m_device.getQueue(m_presentQueueFamily, 0);

    // 6. VMA
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice = static_cast<VkPhysicalDevice>(m_physicalDevice);
    allocatorInfo.device = static_cast<VkDevice>(m_device);
    allocatorInfo.instance = static_cast<VkInstance>(m_instance);
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    vmaCreateAllocator(&allocatorInfo, &m_allocator);

    // 7. Command Pool
    m_shortLivedCommandPool = m_device.createCommandPool({ vk::CommandPoolCreateFlagBits::eTransient, m_graphicsQueueFamily });
    
    BB_CORE_INFO("VulkanContext: Initialisation terminée.");
}

void VulkanContext::cleanup() {
    BB_CORE_TRACE("VulkanContext: Starting cleanup...");
    if (m_device) {
        m_device.waitIdle();
        if (m_shortLivedCommandPool) {
            m_device.destroyCommandPool(m_shortLivedCommandPool);
            BB_CORE_TRACE("VulkanContext: Destroyed short-lived command pool.");
        }
        if (m_allocator) {
            vmaDestroyAllocator(m_allocator);
            BB_CORE_TRACE("VulkanContext: Destroyed VMA Allocator.");
        }
        m_device.destroy();
        BB_CORE_TRACE("VulkanContext: Destroyed Logical Device.");
    }
    if (m_surface) {
        m_instance.destroySurfaceKHR(m_surface);
        BB_CORE_TRACE("VulkanContext: Destroyed Surface.");
    }
    if (m_debugMessenger) {
        m_instance.destroyDebugUtilsMessengerEXT(m_debugMessenger);
        BB_CORE_TRACE("VulkanContext: Destroyed Debug Messenger.");
    }
    if (m_instance) {
        m_instance.destroy();
        BB_CORE_TRACE("VulkanContext: Destroyed Vulkan Instance.");
    }

    m_device = nullptr;
    m_surface = nullptr;
    m_instance = nullptr;
    m_allocator = nullptr;
}

vk::CommandBuffer VulkanContext::beginSingleTimeCommands() {
    vk::CommandBufferAllocateInfo allocInfo(m_shortLivedCommandPool, vk::CommandBufferLevel::ePrimary, 1);
    vk::CommandBuffer commandBuffer = m_device.allocateCommandBuffers(allocInfo)[0];
    commandBuffer.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
    return commandBuffer;
}

void VulkanContext::endSingleTimeCommands(vk::CommandBuffer commandBuffer) {
    commandBuffer.end();
    vk::SubmitInfo submitInfo(0, nullptr, nullptr, 1, &commandBuffer);
    m_graphicsQueue.submit(submitInfo, nullptr);
    m_graphicsQueue.waitIdle();
    m_device.freeCommandBuffers(m_shortLivedCommandPool, commandBuffer);
}

} // namespace bb3d
