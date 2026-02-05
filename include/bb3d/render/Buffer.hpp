#pragma once
#include "bb3d/core/Core.hpp"
#include "bb3d/render/VulkanContext.hpp"

namespace bb3d {

class Buffer {
public:
    Buffer(VulkanContext& context, vk::DeviceSize size, vk::BufferUsageFlags usage, VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags allocFlags = 0);
    ~Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    void upload(const void* data, vk::DeviceSize size, vk::DeviceSize offset = 0);
    
    [[nodiscard]] inline vk::Buffer getHandle() const { return m_buffer; }
    [[nodiscard]] inline vk::DeviceSize getSize() const { return m_size; }
    [[nodiscard]] inline void* getMappedData() const { return m_mappedData; }

    static Scope<Buffer> CreateVertexBuffer(VulkanContext& context, const void* data, vk::DeviceSize size);
    static Scope<Buffer> CreateIndexBuffer(VulkanContext& context, const void* data, vk::DeviceSize size);

private:
    VulkanContext& m_context;
    vk::DeviceSize m_size;
    vk::Buffer m_buffer;
    VmaAllocation m_allocation = nullptr;
    void* m_mappedData = nullptr;
};

} // namespace bb3d