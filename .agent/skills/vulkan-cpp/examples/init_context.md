# Exemple de structure C++ pour initialiser Vulkan (Vulkan-Hpp & VMA)

```cpp
#pragma once
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>

class VulkanContext {
public:
    VulkanContext();
    ~VulkanContext();

private:
   vk::Instance m_instance;
   vk::PhysicalDevice m_physicalDevice;
   vk::Device m_device;
   VmaAllocator m_allocator;

   // Setup functions et Debug Messenger
};
```
