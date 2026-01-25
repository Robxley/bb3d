#include "bb3d/scene/Scene.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/scene/Components.hpp"

namespace bb3d {

Entity Scene::createEntity(const std::string& name) {
    Entity entity(m_registry.create(), *this);
    if (!name.empty()) {
        entity.add<TagComponent>(name);
    }
    // Chaque entité a au moins un transform par défaut
    entity.add<TransformComponent>();
    return entity;
}

void Scene::destroyEntity(Entity entity) {
    m_registry.destroy(static_cast<entt::entity>(entity));
}

} // namespace bb3d