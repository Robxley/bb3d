#pragma once

#include "bb3d/scene/Scene.hpp"
#include <entt/entt.hpp>

namespace bb3d {

/**
 * @brief Wrapper léger autour d'un identifiant d'entité EnTT.
 * 
 * Fournit une interface orientée objet pour manipuler les composants d'une entité.
 */
class Entity {
public:
    Entity() = default;
    Entity(entt::entity handle, Scene& scene) : m_entityHandle(handle), m_scene(&scene) {}

    /** @brief Ajoute un composant à l'entité. */
    template<typename T, typename... Args>
    T& add(Args&&... args) {
        return m_scene->m_registry.emplace<T>(m_entityHandle, std::forward<Args>(args)...);
    }

    /** @brief Récupère un composant de l'entité. */
    template<typename T>
    T& get() {
        return m_scene->m_registry.get<T>(m_entityHandle);
    }

    /** @brief Vérifie si l'entité possède un composant. */
    template<typename T>
    bool has() {
        return m_scene->m_registry.all_of<T>(m_entityHandle);
    }

    /** @brief Supprime un composant de l'entité. */
    template<typename T>
    void remove() {
        m_scene->m_registry.remove<T>(m_entityHandle);
    }

    /** @brief Vérifie si l'entité est valide. */
    operator bool() const { return m_entityHandle != entt::null && m_scene != nullptr; }
    
    /** @brief Conversion implicite vers l'identifiant EnTT. */
    operator entt::entity() const { return m_entityHandle; }

private:
    entt::entity m_entityHandle{ entt::null };
    Scene* m_scene = nullptr;
};

} // namespace bb3d