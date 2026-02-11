#include "bb3d/render/StagingBuffer.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/core/Log.hpp"

namespace bb3d {

StagingBuffer::StagingBuffer(VulkanContext& context, vk::DeviceSize size)
    : m_context(context), m_size(size) {
    
    m_buffer = CreateScope<Buffer>(m_context, size, 
        vk::BufferUsageFlagBits::eTransferSrc, 
        VMA_MEMORY_USAGE_CPU_ONLY, 
        VMA_ALLOCATION_CREATE_MAPPED_BIT);
    
    BB_CORE_INFO("StagingBuffer initialized with {0} MB.", size / (1024 * 1024));
}

StagingBuffer::~StagingBuffer() {
    m_buffer.reset();
}

StagingBuffer::Allocation StagingBuffer::allocate(vk::DeviceSize size) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Alignement simple sur 4 octets
    vk::DeviceSize alignedSize = (size + 3) & ~3;

    if (m_offset + alignedSize > m_size) {
        // Pour l'instant, on fait un reset brutal (synchrone) si on dépasse la taille
        // Dans une version future, on utilisera des fences pour libérer par blocs.
#if defined(BB3D_DEBUG)
        BB_CORE_WARN("StagingBuffer: Buffer full, waiting for GPU idle and resetting offset.");
#endif
        m_context.getDevice().waitIdle();
        m_offset = 0;
    }

    if (alignedSize > m_size) {
        throw std::runtime_error("StagingBuffer: Requested allocation size exceeds total buffer size!");
    }

    Allocation alloc;
    alloc.buffer = m_buffer->getHandle();
    alloc.offset = m_offset;
    alloc.mappedData = static_cast<uint8_t*>(m_buffer->getMappedData()) + m_offset;

    m_offset += alignedSize;
    return alloc;
}

void StagingBuffer::submitCopy(const std::function<void(vk::CommandBuffer, vk::Buffer stagingBuffer, vk::DeviceSize offset)>& copyFunc) {
    // Cette méthode centralise la soumission immédiate
    // On suppose que l'utilisateur a fait son allocation juste avant
    // Dans une version asynchrone, on passerait l'allocation en paramètre.
    vk::CommandBuffer cb = m_context.beginSingleTimeCommands();
    copyFunc(cb, m_buffer->getHandle(), m_offset); 
    m_context.endSingleTimeCommands(cb);
}

} // namespace bb3d
