# Exemple d'initialisation Vulkan 1.3/1.4 avec vk::StructureChain (Vulkan-Hpp)

Cet exemple montre comment configurer proprement les fonctionnalités modernes Vulkan 1.3 et 1.2 (`dynamicRendering`, `synchronization2`, `timelineSemaphore`, `descriptorIndexing`) à l'aide de `vk::StructureChain`.

```cpp
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>
#include <vector>

void VulkanContext::createLogicalDevice(
    vk::PhysicalDevice physicalDevice,
    const std::vector<vk::DeviceQueueCreateInfo>& queueCreateInfos,
    const std::vector<const char*>& deviceExtensions) 
{
    // Chaînage de structures typé et sécurisé pour Vulkan 1.3 & 1.2 Features
    vk::StructureChain<
        vk::DeviceCreateInfo,
        vk::PhysicalDeviceVulkan13Features,
        vk::PhysicalDeviceVulkan12Features,
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT,
        vk::PhysicalDeviceFeatures2
    > chain;

    // 1. Configuration Vulkan 1.3 Features
    auto& v13 = chain.get<vk::PhysicalDeviceVulkan13Features>();
    v13.dynamicRendering = VK_TRUE;
    v13.synchronization2 = VK_TRUE;
    v13.maintenance4 = VK_TRUE;
    v13.inlineUniformBlock = VK_TRUE;

    // 2. Configuration Vulkan 1.2 Features
    auto& v12 = chain.get<vk::PhysicalDeviceVulkan12Features>();
    v12.timelineSemaphore = VK_TRUE;
    v12.descriptorIndexing = VK_TRUE;
    v12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    v12.runtimeDescriptorArray = VK_TRUE;
    v12.bufferDeviceAddress = VK_TRUE;

    // 3. Extended Dynamic State (Vulkan 1.3 Core)
    auto& extDynamic = chain.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
    extDynamic.extendedDynamicState = VK_TRUE;

    // 4. Device Create Info de base
    auto& createInfo = chain.get<vk::DeviceCreateInfo>();
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    // 5. Création du Logical Device
    m_device = physicalDevice.createDevice(chain.get<vk::DeviceCreateInfo>());
    
    // 6. Mise à jour du Dispatcher dynamique si dynamic dispatching actif
    VULKAN_HPP_DEFAULT_DISPATCHER.init(m_device);
}
```
