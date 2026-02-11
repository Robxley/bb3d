#pragma once

#include "bb3d/core/Base.hpp"
#include "bb3d/render/StagingBuffer.hpp"
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>
#include <string>
#include <string_view>

struct SDL_Window;

namespace bb3d {

/**
 * @brief Point d'entrée pour l'abstraction de l'API Vulkan 1.3.
 * 
 * Cette classe gère le cycle de vie des objets fondamentaux :
 * - **Instance & Surface** : Connection avec le système de fenêtrage (SDL3).
 * - **PhysicalDevice & Logical Device** : Sélection du GPU et gestion des files (Queues).
 * - **Vulkan Memory Allocator (VMA)** : Gestionnaire d'allocation mémoire haute performance.
 * - **Validation Layers** : Intégration des outils de debug Vulkan.
 */
class VulkanContext {
public:
    VulkanContext();
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    /**
     * @brief Initialise Vulkan et VMA.
     * @param window Fenêtre SDL3 sur laquelle le rendu sera effectué.
     * @param appName Nom de l'application (utilisé par le driver).
     * @param enableValidationLayers Active/Désactive les couches de debug (impact perf).
     */
    void init(SDL_Window* window, std::string_view appName, bool enableValidationLayers);

    /** @brief Libère proprement tous les objets Vulkan et l'allocateur VMA. */
    void cleanup();

    /** @name Accesseurs Vulkan-Hpp
     * @{
     */
    [[nodiscard]] inline vk::Instance getInstance() const { return m_instance; }
    [[nodiscard]] inline vk::SurfaceKHR getSurface() const { return m_surface; }
    [[nodiscard]] inline vk::PhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
    [[nodiscard]] inline vk::Device getDevice() const { return m_device; }
    [[nodiscard]] inline vk::Queue getGraphicsQueue() const { return m_graphicsQueue; }
    [[nodiscard]] inline vk::Queue getPresentQueue() const { return m_presentQueue; }
    [[nodiscard]] inline vk::Queue getTransferQueue() const { return m_transferQueue; }
    [[nodiscard]] inline uint32_t getGraphicsQueueFamily() const { return m_graphicsQueueFamily; }
    [[nodiscard]] inline uint32_t getPresentQueueFamily() const { return m_presentQueueFamily; }
    [[nodiscard]] inline uint32_t getTransferQueueFamily() const { return m_transferQueueFamily; }
    /** @} */

    /** @brief Récupère l'allocateur VMA pour la création de buffers/images. */
    [[nodiscard]] inline VmaAllocator getAllocator() const { return m_allocator; }
    
    /** @brief Récupère le gestionnaire de staging buffer. */
    [[nodiscard]] StagingBuffer& getStagingBuffer() { return *m_stagingBuffer; }

    /** @brief Nom commercial du GPU utilisé (ex: "NVIDIA GeForce RTX 3080"). */
    [[nodiscard]] inline std::string_view getDeviceName() const { return m_deviceName; }

    /** 
     * @brief Démarre un command buffer temporaire pour un transfert unique (CPU->GPU).
     * @note Utilise une pool de commandes dédiée aux tâches courtes sur la queue graphique.
     */
    vk::CommandBuffer beginSingleTimeCommands();

    /** @brief Soumet et termine un command buffer de transfert, puis attend la fin de l'exécution (bloquant). */
    void endSingleTimeCommands(vk::CommandBuffer commandBuffer);

    /** 
     * @brief Démarre un command buffer sur la file de transfert (si dispo) ou graphique.
     * Idéal pour les uploads de textures en arrière-plan.
     */
    vk::CommandBuffer beginTransferCommands();

    /** 
     * @brief Soumet les commandes de transfert et retourne une Fence signalée à la fin.
     * @param commandBuffer Le buffer à soumettre.
     * @return vk::Fence La fence à surveiller (doit être détruite par l'appelant ou gérée).
     */
    vk::Fence endTransferCommandsAsync(vk::CommandBuffer commandBuffer);

private:
    vk::Instance m_instance;
    vk::DebugUtilsMessengerEXT m_debugMessenger;
    vk::SurfaceKHR m_surface;
    vk::PhysicalDevice m_physicalDevice;
    vk::Device m_device;
    
    vk::Queue m_graphicsQueue;
    vk::Queue m_presentQueue;
    vk::Queue m_transferQueue;
    uint32_t m_graphicsQueueFamily = 0;
    uint32_t m_presentQueueFamily = 0;
    uint32_t m_transferQueueFamily = 0;

    VmaAllocator m_allocator = nullptr;
    vk::CommandPool m_shortLivedCommandPool;
    vk::CommandPool m_transferCommandPool;
    Scope<class StagingBuffer> m_stagingBuffer;
    std::string m_deviceName;
};

} // namespace bb3d