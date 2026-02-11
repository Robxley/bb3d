#include "bb3d/render/Buffer.hpp"
#include "bb3d/render/VulkanContext.hpp"
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
    // 1. Réserver dans le Staging Buffer central
    auto stagingAlloc = context.getStagingBuffer().allocate(size);
    std::memcpy(stagingAlloc.mappedData, data, static_cast<size_t>(size));

    // 2. Vertex Buffer (Device Local)
    auto vertexBuffer = CreateScope<Buffer>(context, size, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, VMA_MEMORY_USAGE_GPU_ONLY);

    // 3. Copy
    vk::CommandBuffer commandBuffer = context.beginSingleTimeCommands();
    vk::BufferCopy copyRegion(stagingAlloc.offset, 0, size);
    commandBuffer.copyBuffer(stagingAlloc.buffer, vertexBuffer->getHandle(), 1, &copyRegion);
    context.endSingleTimeCommands(commandBuffer);

    return vertexBuffer;
}

Scope<Buffer> Buffer::CreateIndexBuffer(VulkanContext& context, const void* data, vk::DeviceSize size) {
    auto stagingAlloc = context.getStagingBuffer().allocate(size);
    std::memcpy(stagingAlloc.mappedData, data, static_cast<size_t>(size));

    auto indexBuffer = CreateScope<Buffer>(context, size, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, VMA_MEMORY_USAGE_GPU_ONLY);

    vk::CommandBuffer commandBuffer = context.beginSingleTimeCommands();
    vk::BufferCopy copyRegion(stagingAlloc.offset, 0, size);
    commandBuffer.copyBuffer(stagingAlloc.buffer, indexBuffer->getHandle(), 1, &copyRegion);
    context.endSingleTimeCommands(commandBuffer);

    return indexBuffer;
}

} // namespace bb3d
