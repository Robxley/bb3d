#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/core/Log.hpp"

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

    if (messageSeverity >= static_cast<VkDebugUtilsMessageSeverityFlagBitsEXT>(vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)) {
        BB_CORE_ERROR("Validation Layer: {}", pCallbackData->pMessage);
    } else if (messageSeverity >= static_cast<VkDebugUtilsMessageSeverityFlagBitsEXT>(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)) {
        BB_CORE_WARN("Validation Layer: {}", pCallbackData->pMessage);
    }
    
    return VK_FALSE;
}

VulkanContext::VulkanContext() = default;

VulkanContext::~VulkanContext() {
    cleanup();
}

void VulkanContext::init(SDL_Window* window, std::string_view appName, bool enableValidationLayers) {
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

    vk::ApplicationInfo appInfo(appName.data(), VK_MAKE_VERSION(1, 0, 0), "biobazard3d", VK_MAKE_VERSION(1, 0, 0), VK_API_VERSION_1_3);

    uint32_t sdlExtensionCount = 0;
    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
    std::vector<const char*> extensions(sdlExtensions, sdlExtensions + sdlExtensionCount);
    if (enableValidationLayers) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    vk::InstanceCreateInfo createInfo({}, &appInfo, 0, nullptr, static_cast<uint32_t>(extensions.size()), extensions.data());
    const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }

    m_instance = vk::createInstance(createInfo);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(m_instance);

    if (enableValidationLayers) {
        vk::DebugUtilsMessengerCreateInfoEXT debugInfo;
        debugInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
        debugInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
        debugInfo.pfnUserCallback = reinterpret_cast<vk::PFN_DebugUtilsMessengerCallbackEXT>(debugCallback);
        m_debugMessenger = m_instance.createDebugUtilsMessengerEXT(debugInfo);
    }

    if (window) {
        VkSurfaceKHR surface;
        if (!SDL_Vulkan_CreateSurface(window, static_cast<VkInstance>(m_instance), nullptr, &surface)) throw std::runtime_error("Failed to create SDL Vulkan Surface");
        m_surface = vk::SurfaceKHR(surface);
    }

    auto physicalDevices = m_instance.enumeratePhysicalDevices();
    for (const auto& device : physicalDevices) {
        auto props = device.getProperties();
        auto queueFamilies = device.getQueueFamilyProperties();
        int gIdx = -1, pIdx = -1;
        for (uint32_t i = 0; i < queueFamilies.size(); ++i) {
            if (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) gIdx = i;
            if (m_surface) { if (device.getSurfaceSupportKHR(i, m_surface)) pIdx = i; } else pIdx = gIdx;
            if (gIdx != -1 && pIdx != -1) break;
        }
        if (gIdx != -1 && pIdx != -1) {
            m_physicalDevice = device; m_graphicsQueueFamily = gIdx; m_presentQueueFamily = pIdx; m_deviceName = props.deviceName.data();
            if (props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) break;
        }
    }

    std::set<uint32_t> uniqueQueueFamilies = { m_graphicsQueueFamily, m_presentQueueFamily };
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    float queuePriority = 1.0f;
    for (uint32_t family : uniqueQueueFamilies) queueCreateInfos.push_back({ {}, family, 1, &queuePriority });

    std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    vk::PhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures(VK_TRUE);
    vk::PhysicalDeviceFeatures deviceFeatures{};
    vk::DeviceCreateInfo deviceCreateInfo({}, static_cast<uint32_t>(queueCreateInfos.size()), queueCreateInfos.data(), 0, nullptr, static_cast<uint32_t>(deviceExtensions.size()), deviceExtensions.data(), &deviceFeatures);
    deviceCreateInfo.pNext = &dynamicRenderingFeatures;

    m_device = m_physicalDevice.createDevice(deviceCreateInfo);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(m_device);

    m_graphicsQueue = m_device.getQueue(m_graphicsQueueFamily, 0);
    m_presentQueue = m_device.getQueue(m_presentQueueFamily, 0);

    // VMA avec pointeurs de fonctions explicites pour le Dispatch Dynamique
    VmaVulkanFunctions vmaVulkanFunctions = {};
    vmaVulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vmaVulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice = static_cast<VkPhysicalDevice>(m_physicalDevice);
    allocatorInfo.device = static_cast<VkDevice>(m_device);
    allocatorInfo.instance = static_cast<VkInstance>(m_instance);
    allocatorInfo.pVulkanFunctions = &vmaVulkanFunctions;
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    vmaCreateAllocator(&allocatorInfo, &m_allocator);

    m_shortLivedCommandPool = m_device.createCommandPool({ vk::CommandPoolCreateFlagBits::eTransient, m_graphicsQueueFamily });
    m_stagingBuffer = CreateScope<StagingBuffer>(*this);
    BB_CORE_INFO("VulkanContext initialized (VMA with dynamic dispatch).");
}

void VulkanContext::cleanup() {
    if (m_device) {
        m_device.waitIdle();
        m_stagingBuffer.reset();
        if (m_shortLivedCommandPool) {
            m_device.destroyCommandPool(m_shortLivedCommandPool);
        }
        if (m_allocator) {
            vmaDestroyAllocator(m_allocator);
            m_allocator = nullptr;
        }
        m_device.destroy();
        m_device = nullptr;
    }
    if (m_surface) { m_instance.destroySurfaceKHR(m_surface); m_surface = nullptr; }
    if (m_debugMessenger) { m_instance.destroyDebugUtilsMessengerEXT(m_debugMessenger); m_debugMessenger = nullptr; }
    if (m_instance) { m_instance.destroy(); m_instance = nullptr; }
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