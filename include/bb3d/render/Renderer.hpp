#pragma once

#include "bb3d/core/Core.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/render/SwapChain.hpp"
#include "bb3d/render/GraphicsPipeline.hpp"
#include "bb3d/render/UniformBuffer.hpp"
#include "bb3d/scene/Scene.hpp"
#include <glm/glm.hpp>
#include <vector>

namespace bb3d {

class Window; // Forward declaration

/**
 * @brief Gère le pipeline de rendu de haut niveau.
 * 
 * S'occupe de la synchronisation Vulkan, de la SwapChain et du rendu
 * automatique de la scène active.
 */
class Renderer {
public:
    Renderer(VulkanContext& context, Window& window, const EngineConfig& config);
    ~Renderer();

    /** @brief Prépare et enregistre les commandes de rendu pour la scène. */
    void render(Scene& scene);

    /** @brief Gère le redimensionnement de la fenêtre. */
    void onResize(int width, int height);

private:
    void createSyncObjects();
    void createGlobalDescriptors();

    VulkanContext& m_context;
    Window& m_window;
    Scope<SwapChain> m_swapChain;
    
    // Pipelines par défaut
    Scope<GraphicsPipeline> m_defaultPipeline;
    Scope<Shader> m_defaultVert;
    Scope<Shader> m_defaultFrag;

    // Frames in flight management
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
    uint32_t m_currentFrame = 0;

    vk::CommandPool m_commandPool;
    std::vector<vk::CommandBuffer> m_commandBuffers;
    std::vector<vk::Semaphore> m_imageAvailableSemaphores;
    std::vector<vk::Semaphore> m_renderFinishedSemaphores;
    std::vector<vk::Fence> m_inFlightFences;

    // Global UBO (Camera, Lights)
    struct GlobalUBO {
        glm::mat4 view;
        glm::mat4 proj;
    };
    Scope<UniformBuffer> m_cameraUbo;
    vk::DescriptorSetLayout m_globalDescriptorLayout;
    vk::DescriptorPool m_descriptorPool;
    std::vector<vk::DescriptorSet> m_globalDescriptorSets;
};

} // namespace bb3d