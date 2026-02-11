#pragma once

#include "bb3d/render/Buffer.hpp"
#include <mutex>
#include <functional>

namespace bb3d {

class VulkanContext; // Forward declaration

/**
 * @brief Gère un buffer de staging persistant pour optimiser les transferts CPU -> GPU.
 * 
 * Utilise un buffer circulaire pour éviter les réallocations et permet de batcher
 * les commandes de transfert.
 */
class StagingBuffer {
public:
    StagingBuffer(VulkanContext& context, vk::DeviceSize size = 64 * 1024 * 1024);
    ~StagingBuffer();

    /** @brief Réserve une zone dans le buffer de staging. */
    struct Allocation {
        vk::Buffer buffer;
        vk::DeviceSize offset;
        void* mappedData;
    };

    Allocation allocate(vk::DeviceSize size);

    /** @brief Soumet une commande de transfert immédiate (bloquante pour l'instant, mais réutilisable). */
    void submitCopy(const std::function<void(vk::CommandBuffer, vk::Buffer stagingBuffer, vk::DeviceSize offset)>& copyFunc);

private:
    VulkanContext& m_context;
    Scope<Buffer> m_buffer;
    vk::DeviceSize m_size;
    vk::DeviceSize m_offset = 0;
    std::mutex m_mutex;
};

} // namespace bb3d
