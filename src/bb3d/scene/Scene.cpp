#include "bb3d/scene/Scene.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/scene/OrbitCamera.hpp"
#include "bb3d/scene/FPSCamera.hpp"
#include "bb3d/core/Engine.hpp"

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

Entity Scene::createOrbitCamera(const std::string& name, float fov, float aspect, const glm::vec3& target, float distance, Engine* engine) {
    auto entity = createEntity(name);
    auto camera = CreateRef<OrbitCamera>(fov, aspect, 0.1f, 1000.0f);
    camera->setTarget(target);
    camera->setDistance(distance);
    entity.add<CameraComponent>(camera);

    Engine* eng = engine ? engine : m_EngineContext;
    if (eng) {
        entity.add<NativeScriptComponent>([eng](Entity e, float /*dt*/) {
            auto& camComp = e.get<CameraComponent>();
            auto* orbit = dynamic_cast<OrbitCamera*>(camComp.camera.get());
            if (!orbit) return;

            auto& input = eng->input();
            if (input.isMouseButtonPressed(Mouse::Left)) {
                glm::vec2 delta = input.getMouseDelta();
                orbit->rotate(delta.x * 5.0f, -delta.y * 5.0f);
            }
            orbit->zoom(input.getMouseScroll().y * 2.0f);
        });
    }
    return entity;
}

Entity Scene::createFPSCamera(const std::string& name, float fov, float aspect, const glm::vec3& position, Engine* engine) {
    auto entity = createEntity(name);
    auto camera = CreateRef<FPSCamera>(fov, aspect, 0.1f, 1000.0f);
    camera->setPosition(position);
    entity.add<CameraComponent>(camera);

    Engine* eng = engine ? engine : m_EngineContext;
    if (eng) {
        entity.add<NativeScriptComponent>([eng](Entity e, float dt) {
            auto& camComp = e.get<CameraComponent>();
            auto* fps = dynamic_cast<FPSCamera*>(camComp.camera.get());
            if (!fps) return;

            auto& input = eng->input();
            
            // Rotation (Clic droit pour capturer)
            if (input.isMouseButtonPressed(Mouse::Right)) {
                glm::vec2 delta = input.getMouseDelta();
                fps->rotate(delta.x * 5.0f, -delta.y * 5.0f);
            }

            // Déplacement
            glm::vec3 dir(0.0f);
            if (input.isKeyPressed(Key::W)) dir.z += 1.0f;
            if (input.isKeyPressed(Key::S)) dir.z -= 1.0f;
            if (input.isKeyPressed(Key::A)) dir.x -= 1.0f;
            if (input.isKeyPressed(Key::D)) dir.x += 1.0f;
            if (input.isKeyPressed(Key::Space)) dir.y += 1.0f; // Monter
            if (input.isKeyPressed(Key::LeftShift)) dir.y -= 1.0f; // Descendre

            fps->move(dir, dt);
        });
    }
    return entity;
}

Entity Scene::createModelEntity(const std::string& name, const std::string& path, const glm::vec3& position, const glm::vec3& normalizeSize) {
    if (!m_EngineContext) {
        BB_CORE_ERROR("Scene: Cannot load model '{0}', Engine context is missing!", name);
        return {}; 
    }

    auto entity = createEntity(name);
    entity.at(position);

    try {
        auto model = m_EngineContext->assets().load<Model>(path);
        
        if (normalizeSize.x > 0.0f || normalizeSize.y > 0.0f || normalizeSize.z > 0.0f) {
            model->normalize(normalizeSize);
        }

        entity.add<ModelComponent>(model, path);
    } catch (const std::exception& e) {
        BB_CORE_ERROR("Scene: Failed to load model '{0}': {1}", path, e.what());
        // En cas d'échec, on détruit l'entité vide créée
        destroyEntity(entity);
        return {};
    }

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