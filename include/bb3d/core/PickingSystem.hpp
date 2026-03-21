#pragma once

#include "bb3d/core/Base.hpp"
#include "bb3d/core/Config.hpp"
#include "bb3d/scene/Entity.hpp"
#include <glm/glm.hpp>

namespace bb3d {

class Scene;
class Engine;

/**
 * @brief Système de picking (sélection d'objets à la souris).
 * 
 * Supporte deux backends :
 * - **PhysicsRaycast** : Utilise le raycast Jolt Physics. Zéro coût GPU.
 *   Ne fonctionne que sur les entités avec PhysicsComponent.
 * - **ColorPicking** : Passe GPU dédiée écrivant un entityID par pixel (R32_UINT).
 *   Fonctionne sur tout objet visible. Coût : 1 passe GPU + readback CPU.
 * 
 * Par défaut, toutes les entités sont sélectionnables.
 * Ajouter SelectableComponent{ false } pour rendre une entité non-sélectionnable.
 */
class PickingSystem {
public:
    explicit PickingSystem(PickingMode mode);
    ~PickingSystem() = default;

    /**
     * @brief Sélectionne l'entité sous les coordonnées UV du viewport.
     * @param viewportUV Coordonnées normalisées [0,1] dans le viewport.
     * @param scene Scène contenant les entités.
     * @param engine Référence au moteur (pour accéder à la physique et au renderer).
     * @return L'entité sélectionnée, ou Entity{} si rien n'est trouvé.
     */
    Entity pick(glm::vec2 viewportUV, Scene& scene, Engine& engine);

    /** @brief Vérifie si une entité est sélectionnable (opt-out via SelectableComponent). */
    static bool isSelectable(Entity entity);

    /** @brief Change le mode de picking à la volée. */
    void setMode(PickingMode mode) { m_mode = mode; }

    /** @brief Retourne le mode de picking actif. */
    [[nodiscard]] PickingMode getMode() const { return m_mode; }

private:
    /** @brief Picking par raycasting physique (Jolt). */
    Entity pickPhysicsRaycast(glm::vec2 viewportUV, Scene& scene, Engine& engine);

    /** @brief Picking par intersection AABB CPU (fallback pour entités sans physique). */
    Entity pickAABBFallback(glm::vec3 rayOrigin, glm::vec3 rayDir, Scene& scene);

    PickingMode m_mode;
};

} // namespace bb3d
