#pragma once

#include "bb3d/core/Core.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/render/SwapChain.hpp"
#include "bb3d/render/GraphicsPipeline.hpp"
#include "bb3d/render/UniformBuffer.hpp"
#include "bb3d/scene/Scene.hpp"
#include "bb3d/render/Material.hpp"
#include "bb3d/render/Mesh.hpp"
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
    std::unordered_map<MaterialType, Scope<GraphicsPipeline>> m_pipelines;
    std::unordered_map<MaterialType, vk::DescriptorSetLayout> m_layouts;
    
    // Shaders cache (to avoid reloading)
    std::unordered_map<std::string, Scope<Shader>> m_shaders;

    // Frames
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;
    uint32_t m_currentFrame = 0;

    vk::CommandPool m_commandPool;
    std::vector<vk::CommandBuffer> m_commandBuffers;
    std::vector<vk::Semaphore> m_imageAvailableSemaphores;
    std::vector<vk::Semaphore> m_renderFinishedSemaphores;
    std::vector<vk::Fence> m_inFlightFences;

    // Global UBO
#pragma warning(push)
#pragma warning(disable: 4324)
    struct ShaderLight {
        glm::vec4 position;  // xyz = pos, w = type (0=Dir, 1=Point)
        glm::vec4 color;     // rgb = color, a = intensity
        glm::vec4 direction; // xyz = dir, w = unused
        glm::vec4 params;    // x = range, y = spotAngle, zw = unused
    };

    struct GlobalUBO {
        glm::mat4 view;
        glm::mat4 proj;
        glm::vec4 camPos;      // .xyz = pos, .w = padding
        glm::vec4 globalParams; // .x = numLights (cast to int), .yzw = padding
        ShaderLight lights[10];
    };
#pragma warning(pop)
    std::vector<Scope<UniformBuffer>> m_cameraUbos;
    vk::DescriptorSetLayout m_globalDescriptorLayout;
    std::vector<vk::DescriptorSet> m_globalDescriptorSets;

    // Materials
    vk::DescriptorPool m_descriptorPool; 
    
    // Cache pour compatibilité avec les Mesh sans Material explicite
    std::unordered_map<std::string, Ref<Material>> m_defaultMaterials;

    Scope<Mesh> m_skyboxCube;
    Ref<SkyboxMaterial> m_internalSkyboxMat;
    Ref<SkySphereMaterial> m_internalSkySphereMat;
    Ref<Material> m_fallbackMaterial;

    void createPipelines(const EngineConfig& config);
    vk::DescriptorSetLayout getLayoutForType(MaterialType type);
    void renderSkybox(vk::CommandBuffer cb, Scene& scene);
};

} // namespace bb3d