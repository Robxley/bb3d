#pragma once
#include "bb3d/core/Core.hpp"
#include "bb3d/render/VulkanContext.hpp"

namespace bb3d {

class Buffer {
public:
    Buffer(VulkanContext& context, VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags allocFlags = 0);
    ~Buffer();

    // Empêcher la copie
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    void upload(const void* data, VkDeviceSize size);
    
    [[nodiscard]] VkBuffer getHandle() const { return m_buffer; }
    [[nodiscard]] VkDeviceSize getSize() const { return m_size; }
    [[nodiscard]] void* getMappedData() const { return m_mappedData; }

    // Utilitaire pour créer un buffer de sommets via un staging buffer
    static Scope<Buffer> CreateVertexBuffer(VulkanContext& context, const void* data, VkDeviceSize size);

private:
    VulkanContext& m_context;
    VkDeviceSize m_size;
    VkBuffer m_buffer{VK_NULL_HANDLE};
    VmaAllocation m_allocation{VK_NULL_HANDLE};
    void* m_mappedData{nullptr}; // Pointeur persistant si mappé
};

} // namespace bb3d
