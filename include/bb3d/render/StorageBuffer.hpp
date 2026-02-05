#pragma once
#include "bb3d/render/Buffer.hpp"

namespace bb3d {

/**
 * @brief ReprÃ©sente un buffer de stockage (Shader Storage Buffer Object - SSBO).
 * 
 * Permet de stocker de grandes quantitÃ©s de donnÃ©es accessibles par les shaders (lecture/Ã©criture).
 */
class StorageBuffer : public Buffer {
public:
    StorageBuffer(VulkanContext& context, vk::DeviceSize size)
        : Buffer(context, size, 
                 vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, 
                 VMA_MEMORY_USAGE_CPU_TO_GPU, 
                 VMA_ALLOCATION_CREATE_MAPPED_BIT) {}
    
    ~StorageBuffer() = default;

    inline void update(const void* data, vk::DeviceSize size) {
        upload(data, size);
    }
};

} // namespace bb3d
