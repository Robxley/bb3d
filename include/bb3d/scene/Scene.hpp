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
    
    /** 
     * @brief Crée une caméra orbitale pré-configurée avec contrôles souris.
     * @param engine (Optionnel) Engine pour l'input. Utilise le contexte de scène si null.
     */
    View<OrbitControllerComponent> createOrbitCamera(const std::string& name, float fov, float aspect, const glm::vec3& target, float distance, Engine* engine = nullptr);

    /** 
     * @brief Crée une caméra FPS pré-configurée avec contrôles ZQSD + Souris.
     * @param engine (Optionnel) Engine pour l'input. Utilise le contexte de scène si null.
     */
    View<FPSControllerComponent> createFPSCamera(const std::string& name, float fov, float aspect, const glm::vec3& position, Engine* engine = nullptr);

    /**
     * @brief Charge un modèle 3D et crée une entité correspondante.
     * @param name Nom de l'entité.
     * @param path Chemin vers le fichier modèle (.obj, .gltf, .glb).
     * @param position Position initiale dans le monde.
     * @param normalizeSize (Optionnel) Si non nul, redimensionne le modèle pour qu'il tienne dans cette boîte englobante.
     * @return L'entité créée (ou invalide si échec).
     */
    View<ModelComponent> createModelEntity(const std::string& name, const std::string& path, const glm::vec3& position = {0,0,0}, const glm::vec3& normalizeSize = {0,0,0});

    /** 
     * @brief Crée une lumière directionnelle (ex: Soleil).
     * @param rotation Rotation (Euler X,Y,Z en degrés). X=-90 regarde vers le bas.
     */
    View<LightComponent> createDirectionalLight(const std::string& name, const glm::vec3& color, float intensity, const glm::vec3& rotation = {-45.0f, 0.0f, 0.0f});

    /** @brief Crée une lumière ponctuelle (Omni). */
    View<LightComponent> createPointLight(const std::string& name, const glm::vec3& color, float intensity, float range, const glm::vec3& position);

    /** 
     * @brief Crée une SkySphere (environnement panoramique) à partir d'une texture.
     * @return L'entité créée (ou invalide si échec de chargement).
     */
    View<SkySphereComponent> createSkySphere(const std::string& name, const std::string& texturePath);

    /** @brief Supprime une entité de la scène. */
    void destroyEntity(Entity entity);

    /** @brief Accès direct au registre EnTT (pour les systèmes internes). */
    [[nodiscard]] inline entt::registry& getRegistry() { return m_registry; }

    /** @brief Définit le contexte moteur (appelé automatiquement par Engine::CreateScene). */
    void setEngineContext(Engine* engine) { m_EngineContext = engine; }
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