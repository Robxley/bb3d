#pragma once

#include "bb3d/scene/Components.hpp"
#include <entt/entt.hpp>
#include <functional>

namespace bb3d {

class Scene; // Forward declaration

/**
 * @brief Wrapper léger autour d'un identifiant d'entité EnTT.
 * 
 * Fournit une interface orientée objet et fluide pour manipuler les composants.
 */
class Entity {
public:
    Entity() = default;
    Entity(entt::entity handle, Scene& scene) : m_entityHandle(handle), m_scene(&scene) {}

    /** @brief Définit la position de l'entité (raccourci TransformComponent). */
    Entity& at(const glm::vec3& position);

    /** @brief Ajoute un composant à l'entité. Retourne l'entité pour chaînage. */
    template<typename T, typename... Args>
    Entity& add(Args&&... args);

    /** @brief Configure un composant via une lambda. Retourne l'entité pour chaînage. */
    template<typename T>
    Entity& setup(std::function<void(T&)> func);

    /** @brief Récupère un composant de l'entité. */
    template<typename T>
    T& get();

    /** @brief Vérifie si l'entité possède un composant. */
    template<typename T>
    bool has();

    /** @brief Récupère le handle interne EnTT. */
    entt::entity getHandle() const { return m_entityHandle; }

    /** @brief Supprime un composant de l'entité. */
    template<typename T>
    Entity& remove();

    /** @brief Vérifie si l'entité est valide. */
    operator bool() const { return m_entityHandle != entt::null && m_scene != nullptr; }
    
    /** @brief Conversion implicite vers l'identifiant EnTT. */
    operator entt::entity() const { return m_entityHandle; }

private:
    entt::entity m_entityHandle{ entt::null };
    Scene* m_scene = nullptr;
};

} // namespace bb3d
