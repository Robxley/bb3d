#include "bb3d/render/UniformBuffer.hpp"

namespace bb3d {

UniformBuffer::UniformBuffer(VulkanContext& context, vk::DeviceSize size)
    : Buffer(context, size, vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT) {
}

} // namespace bb3d