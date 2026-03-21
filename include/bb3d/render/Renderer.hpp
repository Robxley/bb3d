#pragma once

#include "bb3d/core/Core.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/render/SwapChain.hpp"
#include "bb3d/render/GraphicsPipeline.hpp"
#include "bb3d/render/UniformBuffer.hpp"
#include "bb3d/scene/Scene.hpp"
#include "bb3d/render/Material.hpp"
#include "bb3d/render/Mesh.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/render/RenderTarget.hpp"
#include "bb3d/core/JobSystem.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>

#include "bb3d/scene/Frustum.hpp"

namespace bb3d {

class Window; // Forward declaration

struct RenderCommand {
    MaterialType type;
    Material* material;
    Mesh* mesh;
    glm::mat4 transform;

    bool operator<(const RenderCommand& other) const {
        if (type != other.type) return type < other.type;
        if (material != other.material) return material < other.material;
        return mesh < other.mesh;
    }
};

/**
 * @brief Chef d'orchestre du rendu graphique.
 * 
 * Cette classe implémente un moteur de rendu PBR (Physically Based Rendering) avec :
 * - **Gestion de SwapChain** : Double/Triple buffering automatique.
 * - **Système de Matériaux** : Gestion des shaders PBR, Unlit, Skybox via Descriptors.
 * - **Instancing** : Optimisation automatique via SSBO pour les objets répétés.
 * - **Frustum Culling** : Élimination des objets hors champ.
 * - **Post-Process** : Support optionnel du rendu offscreen avec mise à l'échelle (Render Scale).
 * 
 * @note Utilise MAX_FRAMES_IN_FLIGHT (généralement 3) pour paralléliser CPU et GPU.
 */
class Renderer {
public:
    Renderer(VulkanContext& context, Window& window, JobSystem& jobSystem, const EngineConfig& config);
    ~Renderer();

    /**
     * @brief Exécute le rendu d'une scène complète.
     * @return true si le rendu a pu démarrer (swapchain valide), false sinon.
     */
    bool render(Scene& scene);

    /** @brief Notifie le renderer d'un changement de taille de fenêtre. */
    void onResize(int width, int height);
    
    /** @brief Définit la zone de surbrillance (AABB) pour l'identité sélectionnée. */
    void setHighlightBounds(const AABB& bounds, bool active);

    /** @brief Définit la zone de surbrillance pour l'identité survolée. */
    void setHoveredBounds(const AABB& bounds, bool active);

    /** @brief Récupère la SwapChain actuelle. */
    SwapChain& getSwapChain() { return *m_swapChain; }

    /** @brief Récupère le RenderTarget courant (pour affichage dans ImGui). */
    RenderTarget* getRenderTarget() { return m_renderTarget.get(); }

    [[nodiscard]] vk::ImageView getShadowDepthImageView() const { return m_shadowDepthView; }

    /** 
     * @brief Permet d'injecter des commandes de rendu UI après le rendu principal de la scène.
     * 
     * Cette méthode démarre une nouvelle passe de rendu sur l'image courante de la swapchain
     * avec un LoadOp::Load, permettant à l'UI de se dessiner par-dessus le contenu existant.
     * 
     * @param func Fonction de callback recevant le command buffer Vulkan courant.
     */
    void renderUI(const std::function<void(vk::CommandBuffer)>& func);

    /** 
     * @brief Termine l'enregistrement du Command Buffer et soumet les commandes au GPU. 
     * 
     * Effectue la transition finale de l'image vers PresentSrcKHR et présente le résultat.
     */
    void submitAndPresent();

private:
    void createSyncObjects();
    void createShadowObjects();
    void createGlobalDescriptors();
    void createPipelines(const EngineConfig& config);
    void createCopyPipeline();
    
    Ref<Material> getMaterialForTexture(Ref<Texture> texture);

    VulkanContext& m_context;
    Window& m_window;
    JobSystem& m_jobSystem;
    EngineConfig m_config;
    Scope<SwapChain> m_swapChain;
    Scope<RenderTarget> m_renderTarget;
    
    // Shadow System
    vk::Image m_shadowDepthImage;
    VmaAllocation m_shadowDepthAllocation = nullptr;
    vk::ImageView m_shadowDepthView; // Vue complete 2D Array
    std::vector<vk::ImageView> m_shadowCascadeViews; // Vues individuelles par cascade
    vk::Sampler m_shadowSampler;
    
    Frustum m_frustum;
    
    // Pipelines
    std::unordered_map<MaterialType, Scope<GraphicsPipeline>> m_pipelines;
    std::unordered_map<MaterialType, vk::DescriptorSetLayout> m_layouts;
    
    // Pipeline de copie (Fullscreen Quad)
    Scope<GraphicsPipeline> m_copyPipeline;
    vk::DescriptorSetLayout m_copyLayout;
    std::vector<vk::DescriptorSet> m_copyDescriptorSets;

    // Shaders cache
    std::unordered_map<std::string, Scope<Shader>> m_shaders;

    // Frames
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;
    uint32_t m_currentFrame = 0;
    bool m_frameStarted = false;

    // Gestion du resize différé
    bool m_resizeRequested = false;
    int m_pendingWidth = 0;
    int m_pendingHeight = 0;

    vk::CommandPool m_commandPool;
    std::vector<vk::CommandBuffer> m_commandBuffers;
    std::vector<vk::Semaphore> m_imageAvailableSemaphores;
    std::vector<vk::Semaphore> m_renderFinishedSemaphores;
    std::vector<vk::Fence> m_inFlightFences;
    
    // Suivi des fences par image de swapchain
    std::vector<vk::Fence> m_imagesInUseFences;

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
        glm::mat4 shadowCascades[4];
        glm::vec4 shadowSplitDepths;
        glm::vec4 camPos;      // .xyz = pos, .w = padding
        glm::vec4 globalParams; // .x = numLights (cast to int), .yzw = padding
        glm::vec4 ambientColor; // .xyz = color, .w = intensity
        ShaderLight lights[10];
    };
#pragma warning(pop)
    std::vector<Scope<UniformBuffer>> m_cameraUbos;
    
    // Instancing SSBOs
    static constexpr uint32_t MAX_INSTANCES = 10000;
    std::vector<Scope<Buffer>> m_instanceBuffers;

    vk::DescriptorSetLayout m_globalDescriptorLayout;
    std::vector<vk::DescriptorSet> m_globalDescriptorSets;

    // Materials
    vk::DescriptorPool m_descriptorPool; 
    
    // Cache pour compatibilité avec les Mesh sans Material explicite
    std::unordered_map<std::string, Ref<Material>> m_defaultMaterials;

    // Optimisation : Éviter les réallocations par frame
    std::vector<RenderCommand> m_renderCommands;
    std::vector<glm::mat4> m_instanceTransforms;
    std::mutex m_commandMutex;

    Scope<Mesh> m_skyboxCube;
    Scope<Mesh> m_particleQuad;
    Ref<SkyboxMaterial> m_internalSkyboxMat;
    Ref<SkySphereMaterial> m_internalSkySphereMat;
    Ref<ParticleMaterial> m_defaultParticleMat;
    Ref<Material> m_fallbackMaterial;

    // Highlight
    Scope<Mesh> m_highlightCube;
    Ref<UnlitMaterial> m_highlightMat;
    bool m_highlightActive = false;
    glm::mat4 m_highlightTransform = glm::mat4(1.0f);

    // Hover Highlight
    Ref<UnlitMaterial> m_hoveredMat;
    bool m_hoveredActive = false;
    glm::mat4 m_hoveredTransform = glm::mat4(1.0f);

    void renderSkybox(vk::CommandBuffer cb, Scene& scene);
    void drawScene(vk::CommandBuffer cb, Scene& scene, vk::ImageView colorView, vk::ImageView depthView, vk::Extent2D extent);
    void compositeToSwapchain(vk::CommandBuffer cb, uint32_t imageIndex);
    void renderShadows(vk::CommandBuffer cb, Scene& scene, GlobalUBO& uboData);
    void updateGlobalUBO(uint32_t currentFrame, Scene& scene);
};

} // namespace bb3d
