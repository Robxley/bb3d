#pragma once

#include "bb3d/core/Base.hpp"
#include <entt/entt.hpp>
#include <string>

namespace bb3d {

class Entity;

/**
 * @brief Conteneur logique pour tous les objets (Entités) du monde.
 * 
 * Utilise EnTT pour la gestion performante des composants (ECS).
 */
class Scene {
public:
    Scene() = default;
    ~Scene() = default;

    /** @brief Crée une nouvelle entité dans cette scène. */
    Entity createEntity(const std::string& name = "Entity");
    
    /** @brief Supprime une entité de la scène. */
    void destroyEntity(Entity entity);

    /** @brief Accès direct au registre EnTT (pour les systèmes internes). */
    [[nodiscard]] inline entt::registry& getRegistry() { return m_registry; }

private:
    entt::registry m_registry;
    friend class Entity;
};

} // namespace bb3d