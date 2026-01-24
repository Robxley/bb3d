#include "bb3d/render/UniformBuffer.hpp"

namespace bb3d {

UniformBuffer::UniformBuffer(VulkanContext& context, VkDeviceSize size)
    : Buffer(context, size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT) {
}

void UniformBuffer::update(const void* data, VkDeviceSize size) {
    upload(data, size);
}

} // namespace bb3d
