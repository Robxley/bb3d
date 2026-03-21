#pragma once

#include "bb3d/core/Base.hpp"
#include "bb3d/core/Config.hpp" // For FogType
#include "bb3d/render/Texture.hpp"
#include "bb3d/scene/EntityView.hpp"
#include <entt/entt.hpp>
#include <string>
#include <glm/glm.hpp>

namespace bb3d {

class Entity;
class Engine; // Forward declaration
struct FPSControllerComponent;
struct OrbitControllerComponent;
struct ModelComponent;
struct LightComponent;
struct SkySphereComponent;

struct FogSettings {
    glm::vec3 color = { 0.5f, 0.5f, 0.5f };
    float density = 0.01f;
    FogType type = FogType::None;
};

/**
 * @brief Logical container for all objects (Entities) in the world.
 * 
 * Uses EnTT for high-performance component management (ECS).
 */
class Scene {
public:
    Scene() = default;
    ~Scene() = default;

    /** @brief Updates the scene logic (systems, components). */
    void onUpdate(float deltaTime);

    /** @brief Creates a new empty entity with a default TransformComponent. */
    Entity createEntity(const std::string& name = "Entity");
    
    /** 
     * @brief Creates a pre-configured orbit camera with mouse controls.
     * @param engine Pointer to the Engine (can be null if set via setEngineContext).
     */
    View<OrbitControllerComponent> createOrbitCamera(const std::string& name, float fov, float aspect, const glm::vec3& target, float distance, Engine* engine = nullptr);

    /** 
     * @brief Creates a pre-configured FPS camera with WASD + Mouse controls.
     * @param engine Pointer to the Engine (can be null if set via setEngineContext).
     */
    View<FPSControllerComponent> createFPSCamera(const std::string& name, float fov, float aspect, const glm::vec3& position, Engine* engine = nullptr);

    /**
     * @brief Loads a 3D model and creates a corresponding entity.
     * @param name Name of the entity.
     * @param path Path to the model file (.obj, .gltf, .glb).
     * @param position Initial position in the world.
     * @param normalizeSize (Optional) If > 0, normalizes the model scale to fit within this volume.
     * @return View on the created ModelComponent.
     */
    View<ModelComponent> createModelEntity(const std::string& name, const std::string& path, const glm::vec3& position = {0,0,0}, const glm::vec3& normalizeSize = {0,0,0});

    /** 
     * @brief Creates a directional light (e.g., Sun).
     * @param rotation Rotation (Euler X,Y,Z in degrees). X=-90 looks down.
     */
    View<LightComponent> createDirectionalLight(const std::string& name, const glm::vec3& color, float intensity, const glm::vec3& rotation = {-45.0f, 0.0f, 0.0f});

    /** @brief Creates a point light (Omni-directional). */
    View<LightComponent> createPointLight(const std::string& name, const glm::vec3& color, float intensity, float range, const glm::vec3& position);

    /** 
     * @brief Creates a SkySphere (panoramic environment) from an HDR/JPG texture.
     * @return View on the SkySphereComponent.
     */
    View<SkySphereComponent> createSkySphere(const std::string& name, const std::string& texturePath, bool flipY = false);

    /** @brief Removes an entity and its components from the registry. */
    void destroyEntity(Entity entity);

    /** @brief Removes all entities from the scene. */
    void clear();

    /** @brief Direct access to the EnTT registry (for advanced internal systems). */
    [[nodiscard]] inline entt::registry& getRegistry() { return m_registry; }

    /** @brief Sets the engine context (called automatically by Engine::CreateScene). */
    void setEngineContext(Engine* engine) { m_EngineContext = engine; }
    
    /** @brief Retrieves the associated engine context. */
    Engine* getEngineContext() const { return m_EngineContext; }

    // --- Scene Environment ---
    void setSkybox(Ref<Texture> skybox) { m_Skybox = skybox; }
    Ref<Texture> getSkybox() const { return m_Skybox; }

    void setFog(const FogSettings& settings) { m_Fog = settings; }
    const FogSettings& getFog() const { return m_Fog; }

private:
    entt::registry m_registry;
    Ref<Texture> m_Skybox;
    FogSettings m_Fog;
    Engine* m_EngineContext = nullptr;
    
    friend class Entity;
    friend class SceneSerializer;
};

} // namespace bb3d

#include "bb3d/scene/Entity.inl"