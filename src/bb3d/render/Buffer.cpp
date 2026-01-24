#include "bb3d/render/Buffer.hpp"
#include "bb3d/core/Log.hpp"
#include <cstring>

namespace bb3d {

Buffer::Buffer(VulkanContext& context, VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags allocFlags)
    : m_context(context), m_size(size) {
    
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = memoryUsage;
    allocInfo.flags = allocFlags;

    VmaAllocationInfo allocationInfoResult;
    if (vmaCreateBuffer(m_context.getAllocator(), &bufferInfo, &allocInfo, &m_buffer, &m_allocation, &allocationInfoResult) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer via VMA!");
    }

    if (allocFlags & VMA_ALLOCATION_CREATE_MAPPED_BIT) {
        m_mappedData = allocationInfoResult.pMappedData;
    }
}

Buffer::~Buffer() {
    // Si mappé persistent, VMA gère l'unmap à la destruction via vmaDestroyBuffer
    if (m_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(m_context.getAllocator(), m_buffer, m_allocation);
    }
}

void Buffer::upload(const void* data, VkDeviceSize size) {
    if (m_mappedData) {
        // Chemin rapide (Persistent Mapping)
        memcpy(m_mappedData, data, size);
    } else {
        // Chemin lent (Map/Unmap)
        void* mappedData;
        vmaMapMemory(m_context.getAllocator(), m_allocation, &mappedData);
        memcpy(mappedData, data, size);
        vmaUnmapMemory(m_context.getAllocator(), m_allocation);
    }
}

Scope<Buffer> Buffer::CreateVertexBuffer(VulkanContext& context, const void* data, VkDeviceSize size) {
    // 1. Créer un Staging Buffer (CPU Visible)
    Buffer stagingBuffer(context, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    stagingBuffer.upload(data, size);

    // 2. Créer le GPU Buffer (Local au GPU)
    auto gpuBuffer = CreateScope<Buffer>(context, size, 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
        VMA_MEMORY_USAGE_GPU_ONLY);

    // 3. Copier via Command Buffer (nécessite une petite infrastructure de commande temporaire)
    // Pour l'instant, on va faire un "One-time submit" (simplifié pour l'étape 3.2)
    
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    // Note: On utilise un Pool temporaire ou on en demande un au contexte.
    // Pour ce test, on va créer un pool local.
    
    VkCommandPool pool;
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = context.getGraphicsQueueFamily();
    vkCreateCommandPool(context.getDevice(), &poolInfo, nullptr, &pool);

    allocInfo.commandPool = pool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(context.getDevice(), &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    
    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, stagingBuffer.getHandle(), gpuBuffer->getHandle(), 1, &copyRegion);
    
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(context.getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(context.getGraphicsQueue());

    vkFreeCommandBuffers(context.getDevice(), pool, 1, &commandBuffer);
    vkDestroyCommandPool(context.getDevice(), pool, nullptr);

    return gpuBuffer;
}

} // namespace bb3d
