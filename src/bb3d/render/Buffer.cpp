#include "bb3d/render/Buffer.hpp"
#include "bb3d/core/Log.hpp"
#include <cstring>

namespace bb3d {

Buffer::Buffer(VulkanContext& context, vk::DeviceSize size, vk::BufferUsageFlags usage, VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags allocFlags)
    : m_context(context), m_size(size) {
    
    vk::BufferCreateInfo bufferInfo({}, size, usage);

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = memoryUsage;
    allocInfo.flags = allocFlags;

    VkBuffer buffer;
    vmaCreateBuffer(m_context.getAllocator(), reinterpret_cast<VkBufferCreateInfo*>(&bufferInfo), &allocInfo, &buffer, &m_allocation, nullptr);
    m_buffer = vk::Buffer(buffer);

    if (allocFlags & VMA_ALLOCATION_CREATE_MAPPED_BIT) {
        VmaAllocationInfo info;
        vmaGetAllocationInfo(m_context.getAllocator(), m_allocation, &info);
        m_mappedData = info.pMappedData;
    }
}

Buffer::~Buffer() {
    if (m_buffer) {
        vmaDestroyBuffer(m_context.getAllocator(), static_cast<VkBuffer>(m_buffer), m_allocation);
        BB_CORE_TRACE("Buffer: Destroyed buffer of size {} bytes.", m_size);
    }
}

void Buffer::upload(const void* data, vk::DeviceSize size, vk::DeviceSize offset) {
    if (m_mappedData) {
        std::memcpy((char*)m_mappedData + offset, data, static_cast<size_t>(size));
    } else {
        // Fallback: map temporary if not already mapped
        void* mapped;
        vmaMapMemory(m_context.getAllocator(), m_allocation, &mapped);
        std::memcpy((char*)mapped + offset, data, static_cast<size_t>(size));
        vmaUnmapMemory(m_context.getAllocator(), m_allocation);
    }
}

Scope<Buffer> Buffer::CreateVertexBuffer(VulkanContext& context, const void* data, vk::DeviceSize size) {
    // 1. Staging Buffer (Host Visible)
    Buffer staging(context, size, vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT);
    staging.upload(data, size);

    // 2. Vertex Buffer (Device Local)
    auto vertexBuffer = CreateScope<Buffer>(context, size, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, VMA_MEMORY_USAGE_GPU_ONLY);

    // 3. Copy
    vk::CommandBuffer commandBuffer = context.beginSingleTimeCommands();
    vk::BufferCopy copyRegion(0, 0, size);
    commandBuffer.copyBuffer(staging.getHandle(), vertexBuffer->getHandle(), 1, &copyRegion);
    context.endSingleTimeCommands(commandBuffer);

    return vertexBuffer;
}

Scope<Buffer> Buffer::CreateIndexBuffer(VulkanContext& context, const void* data, vk::DeviceSize size) {
    Buffer staging(context, size, vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT);
    staging.upload(data, size);

    auto indexBuffer = CreateScope<Buffer>(context, size, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, VMA_MEMORY_USAGE_GPU_ONLY);

    vk::CommandBuffer commandBuffer = context.beginSingleTimeCommands();
    vk::BufferCopy copyRegion(0, 0, size);
    commandBuffer.copyBuffer(staging.getHandle(), indexBuffer->getHandle(), 1, &copyRegion);
    context.endSingleTimeCommands(commandBuffer);

    return indexBuffer;
}

} // namespace bb3d
