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

void Scene::onUpdate(float deltaTime) {
    // 1. Mise à jour des scripts
    auto scriptView = m_registry.view<NativeScriptComponent>();
    for (auto entityHandle : scriptView) {
        auto& script = scriptView.get<NativeScriptComponent>(entityHandle);
        if (script.onUpdate) {
            Entity entity(entityHandle, *this);
            script.onUpdate(entity, deltaTime);
        }
    }

    // 2. Mise à jour des caméras
    auto view = m_registry.view<CameraComponent>();
    for (auto entity : view) {
        auto& cameraComp = view.get<CameraComponent>(entity);
        if (cameraComp.active && cameraComp.camera) {
            cameraComp.camera->update(deltaTime);
        }
    }
}

} // namespace bb3d