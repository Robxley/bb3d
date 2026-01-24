#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/core/Log.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vector>
#include <stdexcept>
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

VulkanContext::~VulkanContext() {
    cleanup();
}

void VulkanContext::init(SDL_Window* window, std::string_view appName, bool enableValidationLayers) {
    // 1. Création de l'Instance
    std::string appNameStr(appName);
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = appNameStr.c_str();
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "biobazard3d";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // Extensions requises par SDL
    Uint32 sdlExtensionCount = 0;
    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
    std::vector<const char*> extensions(sdlExtensions, sdlExtensions + sdlExtensionCount);

    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    // Validation Layers
    const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) {
        BB_CORE_ERROR("VulkanContext: Échec de la création de l'instance !");
        throw std::runtime_error("Échec de la création de l'instance Vulkan !");
    }
    BB_CORE_INFO("VulkanContext: Instance Vulkan créée.");

    // 2. Debug Messenger
    if (enableValidationLayers) {
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = debugCallback;

        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(m_instance, &debugCreateInfo, nullptr, &m_debugMessenger);
            BB_CORE_INFO("VulkanContext: Messager de débogage activé.");
        }
    }

    // 3. Création de la Surface (SDL3)
    if (window) {
        if (!SDL_Vulkan_CreateSurface(window, m_instance, nullptr, &m_surface)) {
             BB_CORE_ERROR("VulkanContext: Échec de la création de la Surface !");
             throw std::runtime_error("Échec de la création de la Surface Vulkan via SDL !");
        }
        BB_CORE_INFO("VulkanContext: Surface Vulkan créée via SDL.");
    } else {
        BB_CORE_WARN("VulkanContext::init appelé sans fenêtre : La Surface n'est pas créée.");
    }

    // 4. Sélection du Physical Device
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        BB_CORE_ERROR("VulkanContext: Aucun GPU supportant Vulkan trouvé !");
        throw std::runtime_error("Aucun GPU supportant Vulkan trouvé !");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    // Choix du GPU
    int graphicsQueueFamily = -1;
    int presentQueueFamily = -1;

    for (const auto& device : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);

        // Recherche des Queues Families
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        int currentGraphics = -1;
        int currentPresent = -1;

        for (const auto& queueFamily : queueFamilies) {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                currentGraphics = i;
            }
            
            VkBool32 presentSupport = false;
            if (m_surface) {
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
                if (presentSupport) {
                    currentPresent = i;
                }
            } else {
                currentPresent = i; // Fallback
            }

            if (currentGraphics != -1 && currentPresent != -1) {
                break;
            }
            i++;
        }

        if (currentGraphics != -1 && currentPresent != -1) {
            m_physicalDevice = device;
            m_deviceName = props.deviceName;
            m_graphicsQueueFamily = static_cast<uint32_t>(currentGraphics);
            m_presentQueueFamily = static_cast<uint32_t>(currentPresent);
            
            graphicsQueueFamily = currentGraphics;
            presentQueueFamily = currentPresent;
            
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                break;
            }
        }
    }

    if (m_physicalDevice == VK_NULL_HANDLE) {
        BB_CORE_ERROR("VulkanContext: Aucun GPU compatible (Graphics + Present) trouvé !");
        throw std::runtime_error("Aucun GPU compatible (Graphics + Present) trouvé !");
    }

    BB_CORE_INFO("VulkanContext: GPU sélectionné : {0}", m_deviceName);

    // 5. Création du Logical Device
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<int> uniqueQueueFamilies = { static_cast<int>(m_graphicsQueueFamily), static_cast<int>(m_presentQueueFamily) };

    float queuePriority = 1.0f;
    for (int queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    // Dynamic Rendering (Vulkan 1.3)
    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeatures{};
    dynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    dynamicRenderingFeatures.dynamicRendering = VK_TRUE;

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = &dynamicRenderingFeatures;
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    
    // Extensions Device
    std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device) != VK_SUCCESS) {
        BB_CORE_ERROR("VulkanContext: Échec de la création du Logical Device !");
        throw std::runtime_error("Échec de la création du Logical Device !");
    }

    vkGetDeviceQueue(m_device, m_graphicsQueueFamily, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_presentQueueFamily, 0, &m_presentQueue);
    BB_CORE_INFO("VulkanContext: Logical Device créé (Queues: Graphics={0}, Present={1})", m_graphicsQueueFamily, m_presentQueueFamily);

    // 6. Initialisation VMA
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = m_physicalDevice;
    allocatorInfo.device = m_device;
    allocatorInfo.instance = m_instance;
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    
    if (vmaCreateAllocator(&allocatorInfo, &m_allocator) != VK_SUCCESS) {
        BB_CORE_ERROR("VulkanContext: Échec de l'initialisation de VMA !");
        throw std::runtime_error("Échec de l'initialisation de VMA !");
    }
    BB_CORE_INFO("VulkanContext: Allocateur VMA initialisé.");

    // 7. Création du Command Pool pour les commandes courtes
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT; // Indique que les buffers seront de courte durée
    poolInfo.queueFamilyIndex = m_graphicsQueueFamily;

    if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_shortLivedCommandPool) != VK_SUCCESS) {
        throw std::runtime_error("VulkanContext: Échec de la création du Command Pool de transfert !");
    }
}

void VulkanContext::cleanup() {
    BB_CORE_INFO("VulkanContext: Début du nettoyage.");

    if (m_shortLivedCommandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_device, m_shortLivedCommandPool, nullptr);
        m_shortLivedCommandPool = VK_NULL_HANDLE;
    }

    if (m_allocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(m_allocator);
        m_allocator = VK_NULL_HANDLE;
        BB_CORE_INFO("VulkanContext: Allocateur VMA détruit.");
    }

    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
        BB_CORE_INFO("VulkanContext: Logical Device détruit.");
    }

    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
        BB_CORE_INFO("VulkanContext: Surface Vulkan détruite.");
    }

    if (m_debugMessenger != VK_NULL_HANDLE) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(m_instance, m_debugMessenger, nullptr);
        }
        m_debugMessenger = VK_NULL_HANDLE;
        BB_CORE_INFO("VulkanContext: Messager de débogage détruit.");
    }

    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
        BB_CORE_INFO("VulkanContext: Instance Vulkan détruite.");
    }
}

VkCommandBuffer VulkanContext::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_shortLivedCommandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void VulkanContext::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_graphicsQueue);

    vkFreeCommandBuffers(m_device, m_shortLivedCommandPool, 1, &commandBuffer);
}

} // namespace bb3d