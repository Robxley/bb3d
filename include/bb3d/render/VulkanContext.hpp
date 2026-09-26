#pragma once

#include "bb3d/core/Base.hpp"
#include "bb3d/render/StagingBuffer.hpp"
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>
#include <string>
#include <string_view>
#include <array>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <span>
#include <vector>

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
     * @{*/
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
    [[nodiscard]] inline vk::PipelineCache getPipelineCache() const { return m_pipelineCache; }
    [[nodiscard]] inline vk::CommandPool getTransferCommandPool() const { return m_transferCommandPool; }
    [[nodiscard]] inline vk::CommandPool getShortLivedCommandPool() const { return m_shortLivedCommandPool; }
    /** @} */


    /** @brief Récupère l'allocateur VMA pour la création de buffers/images. */
    [[nodiscard]] inline VmaAllocator getAllocator() const { return m_allocator; }
    

    /** @brief Récupère le gestionnaire de staging buffer. */
    [[nodiscard]] StagingBuffer& getStagingBuffer() { return *m_stagingBuffer; }

    /** @brief Nom commercial du GPU utilisé (ex: "NVIDIA GeForce RTX 3080"). */
    [[nodiscard]] inline std::string_view getDeviceName() const { return m_deviceName; }

    struct EnabledFeatures {
        bool dynamicRendering = false;
        bool synchronization2 = false;
        bool timelineSemaphore = false;
        bool samplerAnisotropy = false;
        bool pushDescriptor = false;
        bool dynamicRenderingLocalRead = false;
        bool maintenance4 = false;
        bool maintenance5 = false;
        bool maintenance6 = false;
        bool descriptorIndexing = false;
        bool runtimeDescriptorArray = false;
        bool descriptorBindingPartiallyBound = false;
        bool descriptorBindingVariableDescriptorCount = false;
        /** @brief VK_EXT_extended_dynamic_state: enables setCullMode, setFrontFace,
         *         setDepthTestEnable, setDepthWriteEnable, setDepthCompareOp,
         *         setPrimitiveTopology on command buffers (Vulkan 1.3 Core commands). */
        bool extendedDynamicState = false;
    };

    /** @brief Check negotiated API version (e.g. VK_API_VERSION_1_4 or VK_API_VERSION_1_3). */
    [[nodiscard]] inline uint32_t getApiVersion() const noexcept { return m_apiVersion; }

    /** @brief Returns true if Vulkan 1.4 core features are supported and active on the device. */
    [[nodiscard]] inline bool isVulkan14Supported() const noexcept { return m_apiVersion >= VK_API_VERSION_1_4; }

    /** @brief Access snapshot of enabled features. */
    [[nodiscard]] inline const EnabledFeatures& getEnabledFeatures() const noexcept { return m_enabledFeatures; }

    /** 
     * @brief Démarre un command buffer temporaire pour un transfert unique (CPU->GPU).
     * @note Utilise une pool de commandes dédiée aux tâches courtes sur la queue graphique.
     */
    vk::CommandBuffer beginSingleTimeCommands();

    /** @brief Soumet et termine un command buffer de transfert, puis attend la fin de l'exécution (bloquant). */
    void endSingleTimeCommands(vk::CommandBuffer commandBuffer);

    /** @brief Alloue un command buffer primaire non démarré (état initial) depuis le pool court-terme. */
    [[nodiscard]] vk::CommandBuffer allocateShortLivedCommandBuffer();

    /** @brief Libère un command buffer alloué via allocateShortLivedCommandBuffer. */
    void freeShortLivedCommandBuffer(vk::CommandBuffer commandBuffer);

    /** 
     * @brief Démarre un command buffer sur la file de transfert (si dispo) ou graphique.
     * Idéal pour les uploads de textures en arrière-plan.
     */
    vk::CommandBuffer beginTransferCommands();

    /** 
     * @brief Soumet les commandes de transfert de manière asynchrone sur la file de transfert.
     * @param commandBuffer Le buffer à soumettre.
     * @return uint64_t La valeur cible du Timeline Semaphore signalée à l'achèvement GPU (B11 résolu).
     */
    uint64_t endTransferCommandsAsync(vk::CommandBuffer commandBuffer);

    /** @brief Récupère la valeur maximale actuellement complétée par le GPU sur le Timeline Semaphore de transfert. */
    [[nodiscard]] uint64_t getCompletedTransferTimelineValue() const;

    /** @brief Récupère la dernière valeur soumise sur le Timeline Semaphore de transfert. */
    [[nodiscard]] uint64_t getTransferTimelineValue() const noexcept { return m_transferTimelineValue.load(std::memory_order_relaxed); }

    /** @brief Handle du Timeline Semaphore de transfert (pour synchronisation inter-queues GPU-GPU). */
    [[nodiscard]] vk::Semaphore getTransferTimelineSemaphore() const noexcept { return m_transferTimelineSemaphore; }

    /** @brief Attente CPU bloquante jusqu'à ce que la valeur timeline de transfert soit atteinte. */
    void waitTransferTimeline(uint64_t value, uint64_t timeoutNs = std::numeric_limits<uint64_t>::max()) const;

    /** @brief Recyclage non-bloquant des command buffers de transfert terminés. */
    void pollTransferCompletions();

    /** @brief Returns true if VK_EXT_debug_utils is supported and enabled. */
    [[nodiscard]] inline bool isDebugUtilsSupported() const noexcept { return m_debugUtilsSupported; }

    /** @brief Assigns a human-readable debug name to a Vulkan object for tools like RenderDoc/Nsight/validation layers. */
    void setDebugObjectName(uint64_t objectHandle, vk::ObjectType objectType, std::string_view name);

    /** @brief Type-safe template helper for setDebugObjectName. */
    template <typename T>
    void setObjectName(T handle, std::string_view name) {
        setDebugObjectName(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(static_cast<typename T::NativeType>(handle))), T::objectType, name);
    }

    /** @brief Inserts an open debug label in a command buffer. */
    void cmdBeginDebugLabel(vk::CommandBuffer cb, std::string_view name, std::array<float, 4> color = {0.2f, 0.6f, 1.0f, 1.0f});

    /** @brief Ends the most recent debug label in a command buffer. */
    void cmdEndDebugLabel(vk::CommandBuffer cb);

    /** @brief Inserts an instantaneous debug label marker in a command buffer. */
    void cmdInsertDebugLabel(vk::CommandBuffer cb, std::string_view name, std::array<float, 4> color = {0.8f, 0.8f, 0.8f, 1.0f});

    /** @brief Checks if a binary blob matches a valid Vulkan PipelineCache header for the active physical device and driver. */
    [[nodiscard]] bool isPipelineCacheValid(std::span<const uint8_t> data) const noexcept;

    /** @brief Atomically writes the current vk::PipelineCache data to disk. Creates parent directories if missing. */
    [[nodiscard]] bool savePipelineCache(const std::filesystem::path& path) const;

    /** 
     * @brief Loads binary cache data from disk and merges/initializes it into the current vk::PipelineCache.
     * @note Must be called from the main render thread or externally synchronized with pipeline creation.
     */
    [[nodiscard]] bool loadPipelineCache(const std::filesystem::path& path);

    /** @brief Sets the default file path for the persistent pipeline cache. */
    void setPipelineCachePath(std::string_view path);

    /** @brief Gets the current default pipeline cache file path. */
    [[nodiscard]] const std::filesystem::path& getPipelineCachePath() const noexcept { return m_pipelineCachePath; }

    /** @brief Enables or disables disk persistence for vk::PipelineCache. */
    void setPipelineCacheEnabled(bool enabled) noexcept { m_enablePipelineCache = enabled; }

    /** @brief Returns true if pipeline cache disk persistence is enabled. */
    [[nodiscard]] bool isPipelineCacheEnabled() const noexcept { return m_enablePipelineCache; }

private:
    void pollTransferCompletionsLocked();

    bool m_debugUtilsSupported = false;
    std::filesystem::path m_pipelineCachePath = "assets/cache/pipelines.bin";
    bool m_enablePipelineCache = true;

    vk::Instance m_instance;
    vk::DebugUtilsMessengerEXT m_debugMessenger;
    vk::SurfaceKHR m_surface;
    vk::PhysicalDevice m_physicalDevice;
    vk::Device m_device;
    vk::PipelineCache m_pipelineCache = nullptr;

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
    uint32_t m_apiVersion = VK_API_VERSION_1_3;
    EnabledFeatures m_enabledFeatures;

    // Timeline Semaphore pour la file de transfert (B11)
    vk::Semaphore m_transferTimelineSemaphore = nullptr;
    std::atomic<uint64_t> m_transferTimelineValue{0};

    struct PendingTransfer {
        vk::CommandBuffer commandBuffer;
        uint64_t timelineValue;
    };
    mutable std::mutex m_transferMutex;
    std::vector<PendingTransfer> m_pendingTransfers;
};

} // namespace bb3d
