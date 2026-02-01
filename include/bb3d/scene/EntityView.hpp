#pragma once

#include "bb3d/scene/Entity.hpp"

namespace bb3d {

/**
 * @brief Trait permettant de définir comment accéder à l'objet T depuis une Entity.
 * Par défaut, on considère que T est un Component stocké directement.
 */
template<typename T>
struct ComponentAccessor {
    static T* get(Entity& e) {
        return &e.get<T>();
    }
};

/**
 * @brief Wrapper typé autour d'une Entity (Zero-Overhead).
 * 
 * Permet d'accéder directement aux méthodes de T via l'opérateur ->
 * tout en conservant les méthodes de Entity via l'héritage.
 * 
 * Exemple:
 * View<FPSController> player = scene.createFPSController();
 * player->setSpeed(10.0f); // Accès au composant
 * player.at({0,0,0});      // Accès à l'entité
 */
template<typename T>
class View : public Entity {
public:
    using Entity::Entity; // Hérite des constructeurs

    View(Entity e) : Entity(e) {}

    // Accès direct au composant/objet T
    T* operator->() {
        return ComponentAccessor<T>::get(*this);
    }

    T& operator*() {
        return *ComponentAccessor<T>::get(*this);
    }

    // Permet de repasser en Entity brute si besoin
    Entity toEntity() const { return *this; }
};

} // namespace bb3d
