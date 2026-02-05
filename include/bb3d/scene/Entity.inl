#pragma once
#include "bb3d/scene/Scene.hpp"
#include "bb3d/scene/Components.hpp"

namespace bb3d {

    inline Entity& Entity::at(const glm::vec3& position) {
        get<TransformComponent>().translation = position;
        return *this;
    }

    template<typename T, typename... Args>
    Entity& Entity::add(Args&&... args) {
        m_scene->getRegistry().emplace<T>(m_entityHandle, std::forward<Args>(args)...);
        return *this;
    }

    template<typename T>
    Entity& Entity::setup(std::function<void(T&)> func) {
        func(get<T>());
        return *this;
    }

    template<typename T>
    T& Entity::get() {
        return m_scene->getRegistry().get<T>(m_entityHandle);
    }

    template<typename T>
    bool Entity::has() {
        return m_scene->getRegistry().all_of<T>(m_entityHandle);
    }

    template<typename T>
    Entity& Entity::remove() {
        m_scene->getRegistry().remove<T>(m_entityHandle);
        return *this;
    }

} // namespace bb3d