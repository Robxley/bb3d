#pragma once

#include "bb3d/core/Base.hpp"
#include "bb3d/core/Config.hpp" // For FogType
#include "bb3d/render/Texture.hpp"
#include <entt/entt.hpp>
#include <string>
#include <glm/glm.hpp>

namespace bb3d {

class Entity;

struct FogSettings {
    glm::vec3 color = { 0.5f, 0.5f, 0.5f };
    float density = 0.01f;
    FogType type = FogType::None;
};

/**
 * @brief Conteneur logique pour tous les objets (Entités) du monde.
 * 
 * Utilise EnTT pour la gestion performante des composants (ECS).
 */
class Scene {
public:
    Scene() = default;
    ~Scene() = default;

    /** @brief Met à jour la logique de la scène (systèmes, composants). */
    void onUpdate(float deltaTime);

    /** @brief Crée une nouvelle entité dans cette scène. */
    Entity createEntity(const std::string& name = "Entity");
    
    /** @brief Supprime une entité de la scène. */
    void destroyEntity(Entity entity);

    /** @brief Accès direct au registre EnTT (pour les systèmes internes). */
    [[nodiscard]] inline entt::registry& getRegistry() { return m_registry; }

    // --- Scene Environment ---
    void setSkybox(Ref<Texture> skybox) { m_Skybox = skybox; }
    Ref<Texture> getSkybox() const { return m_Skybox; }

    void setFog(const FogSettings& settings) { m_Fog = settings; }
    const FogSettings& getFog() const { return m_Fog; }

private:
    entt::registry m_registry;
    Ref<Texture> m_Skybox;
    FogSettings m_Fog;
    
    friend class Entity;
    friend class SceneSerializer;
};

} // namespace bb3d