#pragma once
#include "bb3d/render/Buffer.hpp"

namespace bb3d {

/**
 * @brief Buffer spécialisé pour stocker des commandes de dessin indirect (Indirect Draw Commands).
 */
class IndirectBuffer : public Buffer {
public:
    IndirectBuffer(VulkanContext& context, uint32_t maxDrawCount)
        : Buffer(context, maxDrawCount * sizeof(vk::DrawIndexedIndirectCommand), 
                 vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, 
                 VMA_MEMORY_USAGE_CPU_TO_GPU, 
                 VMA_ALLOCATION_CREATE_MAPPED_BIT) {}
    
    ~IndirectBuffer() = default;

    inline void update(const void* data, vk::DeviceSize size) {
        upload(data, size);
    }
};

} // namespace bb3d
