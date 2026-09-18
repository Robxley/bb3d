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
#include <limits>

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

    vk::ApplicationInfo appInfo(appName.data(), VK_MAKE_VERSION(1, 0, 0), "biobazard3d", VK_MAKE_VERSION(1, 0, 0), VK_API_VERSION_1_4);

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
    int bestScore = -1;
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
            int score = 0;
            if (props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) score += 1000;
            else if (props.deviceType == vk::PhysicalDeviceType::eIntegratedGpu) score += 100;

            if (score > bestScore) {
                bestScore = score;
                m_physicalDevice = device; 
                m_graphicsQueueFamily = gIdx; 
                m_presentQueueFamily = pIdx; 
                m_transferQueueFamily = gIdx; // Use graphics queue for better compatibility (Blit/Shaders)
                m_deviceName = props.deviceName.data();
            }
        }
    }

    if (!m_physicalDevice) {
        throw std::runtime_error("VulkanContext: Failed to find a suitable GPU!");
    }

    uint32_t deviceApiVersion = m_physicalDevice.getProperties().apiVersion;
    m_apiVersion = (deviceApiVersion >= VK_API_VERSION_1_4) ? VK_API_VERSION_1_4 : VK_API_VERSION_1_3;
    bool isVulkan14 = (m_apiVersion >= VK_API_VERSION_1_4);

    std::set<uint32_t> uniqueQueueFamilies = { m_graphicsQueueFamily, m_presentQueueFamily, m_transferQueueFamily };
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    float queuePriority = 1.0f;
    for (uint32_t family : uniqueQueueFamilies) queueCreateInfos.push_back({ {}, family, 1, &queuePriority });

    std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    if (isVulkan14) {
        // Query hardware-supported features first to prevent VK_ERROR_FEATURE_NOT_PRESENT
        vk::StructureChain<
            vk::PhysicalDeviceFeatures2,
            vk::PhysicalDeviceVulkan11Features,
            vk::PhysicalDeviceVulkan12Features,
            vk::PhysicalDeviceVulkan13Features,
            vk::PhysicalDeviceVulkan14Features
        > queryChain;

        m_physicalDevice.getFeatures2(&queryChain.get<vk::PhysicalDeviceFeatures2>());

        const auto& supp10 = queryChain.get<vk::PhysicalDeviceFeatures2>().features;
        const auto& supp12 = queryChain.get<vk::PhysicalDeviceVulkan12Features>();
        const auto& supp13 = queryChain.get<vk::PhysicalDeviceVulkan13Features>();
        const auto& supp14 = queryChain.get<vk::PhysicalDeviceVulkan14Features>();

        // Required features check (hard-fail if not supported by hardware)
        if (!supp13.dynamicRendering || !supp13.synchronization2 || !supp12.timelineSemaphore) {
            throw std::runtime_error("VulkanContext: Required core features (DynamicRendering, Synchronization2, TimelineSemaphore) are not supported by the selected GPU!");
        }

        // Build type-safe device creation chain for Vulkan 1.4
        vk::StructureChain<
            vk::DeviceCreateInfo,
            vk::PhysicalDeviceFeatures2,
            vk::PhysicalDeviceVulkan11Features,
            vk::PhysicalDeviceVulkan12Features,
            vk::PhysicalDeviceVulkan13Features,
            vk::PhysicalDeviceVulkan14Features
        > createChain;

        auto& dci = createChain.get<vk::DeviceCreateInfo>();
        dci.setQueueCreateInfos(queueCreateInfos);
        dci.setPEnabledExtensionNames(deviceExtensions);

        // Core 1.0 / Features2
        auto& feat10 = createChain.get<vk::PhysicalDeviceFeatures2>().features;
        if (supp10.samplerAnisotropy) {
            feat10.samplerAnisotropy = VK_TRUE;
            m_enabledFeatures.samplerAnisotropy = true;
        }

        // Core 1.2
        auto& feat12 = createChain.get<vk::PhysicalDeviceVulkan12Features>();
        feat12.timelineSemaphore = VK_TRUE;
        m_enabledFeatures.timelineSemaphore = true;
        if (supp12.descriptorIndexing) {
            feat12.descriptorIndexing = VK_TRUE;
            m_enabledFeatures.descriptorIndexing = true;
        }
        if (supp12.runtimeDescriptorArray) {
            feat12.runtimeDescriptorArray = VK_TRUE;
            m_enabledFeatures.runtimeDescriptorArray = true;
        }
        if (supp12.descriptorBindingPartiallyBound) {
            feat12.descriptorBindingPartiallyBound = VK_TRUE;
            m_enabledFeatures.descriptorBindingPartiallyBound = true;
        }
        if (supp12.descriptorBindingVariableDescriptorCount) {
            feat12.descriptorBindingVariableDescriptorCount = VK_TRUE;
            m_enabledFeatures.descriptorBindingVariableDescriptorCount = true;
        }

        // Core 1.3
        auto& feat13 = createChain.get<vk::PhysicalDeviceVulkan13Features>();
        feat13.dynamicRendering = VK_TRUE;
        feat13.synchronization2 = VK_TRUE;
        m_enabledFeatures.dynamicRendering = true;
        m_enabledFeatures.synchronization2 = true;
        if (supp13.maintenance4) {
            feat13.maintenance4 = VK_TRUE;
            m_enabledFeatures.maintenance4 = true;
        }

        // Core 1.4
        auto& feat14 = createChain.get<vk::PhysicalDeviceVulkan14Features>();
        if (supp14.pushDescriptor) {
            feat14.pushDescriptor = VK_TRUE;
            m_enabledFeatures.pushDescriptor = true;
        }
        if (supp14.dynamicRenderingLocalRead) {
            feat14.dynamicRenderingLocalRead = VK_TRUE;
            m_enabledFeatures.dynamicRenderingLocalRead = true;
        }
        if (supp14.maintenance5) {
            feat14.maintenance5 = VK_TRUE;
            m_enabledFeatures.maintenance5 = true;
        }
        if (supp14.maintenance6) {
            feat14.maintenance6 = VK_TRUE;
            m_enabledFeatures.maintenance6 = true;
        }

        m_device = m_physicalDevice.createDevice(createChain.get<vk::DeviceCreateInfo>());
    } else {
        // Fallback Vulkan 1.3 chain
        vk::StructureChain<
            vk::PhysicalDeviceFeatures2,
            vk::PhysicalDeviceVulkan11Features,
            vk::PhysicalDeviceVulkan12Features,
            vk::PhysicalDeviceVulkan13Features
        > queryChain;

        m_physicalDevice.getFeatures2(&queryChain.get<vk::PhysicalDeviceFeatures2>());

        const auto& supp10 = queryChain.get<vk::PhysicalDeviceFeatures2>().features;
        const auto& supp12 = queryChain.get<vk::PhysicalDeviceVulkan12Features>();
        const auto& supp13 = queryChain.get<vk::PhysicalDeviceVulkan13Features>();

        if (!supp13.dynamicRendering || !supp13.synchronization2 || !supp12.timelineSemaphore) {
            throw std::runtime_error("VulkanContext: Required Vulkan 1.3 core features (DynamicRendering, Synchronization2, TimelineSemaphore) are not supported by the selected GPU!");
        }

        vk::StructureChain<
            vk::DeviceCreateInfo,
            vk::PhysicalDeviceFeatures2,
            vk::PhysicalDeviceVulkan11Features,
            vk::PhysicalDeviceVulkan12Features,
            vk::PhysicalDeviceVulkan13Features
        > createChain;

        auto& dci = createChain.get<vk::DeviceCreateInfo>();
        dci.setQueueCreateInfos(queueCreateInfos);
        dci.setPEnabledExtensionNames(deviceExtensions);

        auto& feat10 = createChain.get<vk::PhysicalDeviceFeatures2>().features;
        if (supp10.samplerAnisotropy) {
            feat10.samplerAnisotropy = VK_TRUE;
            m_enabledFeatures.samplerAnisotropy = true;
        }

        auto& feat12 = createChain.get<vk::PhysicalDeviceVulkan12Features>();
        feat12.timelineSemaphore = VK_TRUE;
        m_enabledFeatures.timelineSemaphore = true;
        if (supp12.descriptorIndexing) {
            feat12.descriptorIndexing = VK_TRUE;
            m_enabledFeatures.descriptorIndexing = true;
        }
        if (supp12.runtimeDescriptorArray) {
            feat12.runtimeDescriptorArray = VK_TRUE;
            m_enabledFeatures.runtimeDescriptorArray = true;
        }
        if (supp12.descriptorBindingPartiallyBound) {
            feat12.descriptorBindingPartiallyBound = VK_TRUE;
            m_enabledFeatures.descriptorBindingPartiallyBound = true;
        }
        if (supp12.descriptorBindingVariableDescriptorCount) {
            feat12.descriptorBindingVariableDescriptorCount = VK_TRUE;
            m_enabledFeatures.descriptorBindingVariableDescriptorCount = true;
        }

        auto& feat13 = createChain.get<vk::PhysicalDeviceVulkan13Features>();
        feat13.dynamicRendering = VK_TRUE;
        feat13.synchronization2 = VK_TRUE;
        m_enabledFeatures.dynamicRendering = true;
        m_enabledFeatures.synchronization2 = true;
        if (supp13.maintenance4) {
            feat13.maintenance4 = VK_TRUE;
            m_enabledFeatures.maintenance4 = true;
        }

        m_device = m_physicalDevice.createDevice(createChain.get<vk::DeviceCreateInfo>());
    }

    VULKAN_HPP_DEFAULT_DISPATCHER.init(m_device);

    m_graphicsQueue = m_device.getQueue(m_graphicsQueueFamily, 0);
    m_presentQueue = m_device.getQueue(m_presentQueueFamily, 0);
    m_transferQueue = m_device.getQueue(m_transferQueueFamily, 0);

    // VMA with explicit function pointers for Dynamic Dispatch and aligned API version
    VmaVulkanFunctions vmaVulkanFunctions = {};
    vmaVulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vmaVulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice = static_cast<VkPhysicalDevice>(m_physicalDevice);
    allocatorInfo.device = static_cast<VkDevice>(m_device);
    allocatorInfo.instance = static_cast<VkInstance>(m_instance);
    allocatorInfo.pVulkanFunctions = &vmaVulkanFunctions;
    allocatorInfo.vulkanApiVersion = m_apiVersion;
    vmaCreateAllocator(&allocatorInfo, &m_allocator);

    // Create pipeline cache for optimized shader compilation
    vk::PipelineCacheCreateInfo cacheInfo{};
    m_pipelineCache = m_device.createPipelineCache(cacheInfo);

    m_shortLivedCommandPool = m_device.createCommandPool({ vk::CommandPoolCreateFlagBits::eTransient, m_graphicsQueueFamily });
    m_transferCommandPool = m_device.createCommandPool({ vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer, m_transferQueueFamily });

    // Create Timeline Semaphore for transfer queue (B11)
    vk::SemaphoreTypeCreateInfo timelineTypeInfo(vk::SemaphoreType::eTimeline, 0);
    vk::SemaphoreCreateInfo timelineSemInfo{};
    timelineSemInfo.pNext = &timelineTypeInfo;
    m_transferTimelineSemaphore = m_device.createSemaphore(timelineSemInfo);
    m_transferTimelineValue.store(0, std::memory_order_release);

    m_stagingBuffer = CreateScope<StagingBuffer>(*this);

    BB_CORE_INFO("VulkanContext initialized: API {}.{}.{} on {} (Vulkan 1.4: {}, PushDescriptors: {}, Sync2: {}, TimelineSemaphores: {}).",
        VK_API_VERSION_MAJOR(m_apiVersion),
        VK_API_VERSION_MINOR(m_apiVersion),
        VK_API_VERSION_PATCH(m_apiVersion),
        m_deviceName,
        isVulkan14,
        m_enabledFeatures.pushDescriptor,
        m_enabledFeatures.synchronization2,
        m_enabledFeatures.timelineSemaphore);
}

void VulkanContext::cleanup() {
    if (m_device) {
        m_device.waitIdle();
        m_stagingBuffer.reset();
        {
            std::lock_guard<std::mutex> lock(m_transferMutex);
            for (const auto& pending : m_pendingTransfers) {
                if (pending.commandBuffer && m_transferCommandPool) {
                    m_device.freeCommandBuffers(m_transferCommandPool, pending.commandBuffer);
                }
            }
            m_pendingTransfers.clear();
        }
        if (m_transferTimelineSemaphore) {
            m_device.destroySemaphore(m_transferTimelineSemaphore);
            m_transferTimelineSemaphore = nullptr;
        }
        if (m_shortLivedCommandPool) {
            m_device.destroyCommandPool(m_shortLivedCommandPool);
        }
        if (m_transferCommandPool) {
            m_device.destroyCommandPool(m_transferCommandPool);
        }
        if (m_allocator) {
            vmaDestroyAllocator(m_allocator);
            m_allocator = nullptr;
        }
        if (m_pipelineCache) {
            m_device.destroyPipelineCache(m_pipelineCache);
            m_pipelineCache = nullptr;
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



vk::CommandBuffer VulkanContext::beginTransferCommands() {
    std::lock_guard<std::mutex> lock(m_transferMutex);
    pollTransferCompletionsLocked();

    vk::CommandBufferAllocateInfo allocInfo(m_transferCommandPool, vk::CommandBufferLevel::ePrimary, 1);
    vk::CommandBuffer commandBuffer = m_device.allocateCommandBuffers(allocInfo)[0];
    commandBuffer.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
    return commandBuffer;
}

uint64_t VulkanContext::endTransferCommandsAsync(vk::CommandBuffer commandBuffer) {
    commandBuffer.end();

    std::lock_guard<std::mutex> lock(m_transferMutex);
    pollTransferCompletionsLocked();

    uint64_t signalValue = ++m_transferTimelineValue;

    vk::TimelineSemaphoreSubmitInfo timelineInfo{};
    timelineInfo.signalSemaphoreValueCount = 1;
    timelineInfo.pSignalSemaphoreValues = &signalValue;

    vk::SubmitInfo submitInfo{};
    submitInfo.pNext = &timelineInfo;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_transferTimelineSemaphore;

    // Submit asynchronously to transfer queue - zero fences, zero blocking wait! (B11 resolved)
    m_transferQueue.submit(submitInfo, nullptr);

    m_pendingTransfers.push_back({ commandBuffer, signalValue });
    return signalValue;
}

uint64_t VulkanContext::getCompletedTransferTimelineValue() const {
    if (!m_device || !m_transferTimelineSemaphore) return 0;
    return m_device.getSemaphoreCounterValue(m_transferTimelineSemaphore);
}

void VulkanContext::waitTransferTimeline(uint64_t value, uint64_t timeoutNs) const {
    if (!m_device || !m_transferTimelineSemaphore || value == 0) return;
    vk::SemaphoreWaitInfo waitInfo(vk::SemaphoreWaitFlags{}, 1, &m_transferTimelineSemaphore, &value);
    (void)m_device.waitSemaphores(waitInfo, timeoutNs);
}

void VulkanContext::pollTransferCompletions() {
    std::lock_guard<std::mutex> lock(m_transferMutex);
    pollTransferCompletionsLocked();
}

void VulkanContext::pollTransferCompletionsLocked() {
    if (!m_device || !m_transferTimelineSemaphore || m_pendingTransfers.empty()) return;

    uint64_t completed = m_device.getSemaphoreCounterValue(m_transferTimelineSemaphore);

    auto it = std::remove_if(m_pendingTransfers.begin(), m_pendingTransfers.end(),
        [this, completed](const PendingTransfer& pt) {
            if (pt.timelineValue <= completed) {
                if (pt.commandBuffer && m_transferCommandPool) {
                    m_device.freeCommandBuffers(m_transferCommandPool, pt.commandBuffer);
                }
                return true;
            }
            return false;
        });

    m_pendingTransfers.erase(it, m_pendingTransfers.end());
}

} // namespace bb3d

