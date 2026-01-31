#pragma once

#include "bb3d/core/Core.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/render/SwapChain.hpp"
#include "bb3d/render/GraphicsPipeline.hpp"
#include "bb3d/render/UniformBuffer.hpp"
#include "bb3d/scene/Scene.hpp"
#include "bb3d/render/Material.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>

namespace bb3d {

class Window; // Forward declaration

/**
 * @brief Gère le pipeline de rendu de haut niveau (PBR).
 */
class Renderer {
public:
    Renderer(VulkanContext& context, Window& window, const EngineConfig& config);
    ~Renderer();

    void render(Scene& scene);
    void onResize(int width, int height);
    
    // Accesseurs pour les tests ou usage avancé
    SwapChain& getSwapChain() { return *m_swapChain; }

private:
    void createSyncObjects();
    void createGlobalDescriptors();
    
    Ref<Material> getMaterialForTexture(Ref<Texture> texture);

    VulkanContext& m_context;
    Window& m_window;
    Scope<SwapChain> m_swapChain;
    
    // Pipelines
    Scope<GraphicsPipeline> m_defaultPipeline; // PBR Pipeline
    Scope<Shader> m_defaultVert;
    Scope<Shader> m_defaultFrag;

    // Frames
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;
    uint32_t m_currentFrame = 0;

    vk::CommandPool m_commandPool;
    std::vector<vk::CommandBuffer> m_commandBuffers;
    std::vector<vk::Semaphore> m_imageAvailableSemaphores;
    std::vector<vk::Semaphore> m_renderFinishedSemaphores;
    std::vector<vk::Fence> m_inFlightFences;

    // Global UBO
    struct GlobalUBO {
        glm::mat4 view;
        glm::mat4 proj;
        alignas(16) glm::vec3 camPos;
    };
    Scope<UniformBuffer> m_cameraUbo;
    vk::DescriptorSetLayout m_globalDescriptorLayout;
    std::vector<vk::DescriptorSet> m_globalDescriptorSets;

    // Materials
    vk::DescriptorSetLayout m_materialLayout;
    vk::DescriptorPool m_descriptorPool; // Pool global (UBOs + Materials)
    
    // Cache pour compatibilité avec les Mesh sans Material explicite
    std::unordered_map<Texture*, Ref<Material>> m_defaultMaterials;
};

} // namespace bb3d