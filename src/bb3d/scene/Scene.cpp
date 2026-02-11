#include "bb3d/scene/Scene.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/scene/Camera.hpp"
#include "bb3d/core/Engine.hpp"
#include <algorithm>

namespace bb3d {


Entity Scene::createEntity(const std::string& name) {
    Entity entity(m_registry.create(), *this);
    if (!name.empty()) {
        entity.add<TagComponent>(name);
    }
    // Chaque entité a au moins un transform par défaut
    entity.add<TransformComponent>();
    
    BB_CORE_INFO("Scene: Created entity '{0}' (ID: {1})", name.empty() ? "Unnamed" : name, (uint32_t)entity.getHandle());
    return entity;
}

View<OrbitControllerComponent> Scene::createOrbitCamera(const std::string& name, float fov, float aspect, const glm::vec3& target, float distance, Engine* /*engine*/) {
    auto entity = createEntity(name);
    
    // 1. Camera (Optique)
    auto camera = CreateRef<Camera>(fov, aspect, 0.1f, 1000.0f);
    entity.add<CameraComponent>(camera);
    auto& cc = entity.get<CameraComponent>();
    cc.fov = fov; cc.aspect = aspect; cc.nearPlane = 0.1f; cc.farPlane = 1000.0f;

    // 2. Controller (Logique)
    entity.add<OrbitControllerComponent>();
    auto& ctrl = entity.get<OrbitControllerComponent>();
    ctrl.target = target;
    ctrl.distance = distance;

    return View<OrbitControllerComponent>(entity);
}

View<FPSControllerComponent> Scene::createFPSCamera(const std::string& name, float fov, float aspect, const glm::vec3& position, Engine* /*engine*/) {
    auto entity = createEntity(name);
    entity.at(position);

    // 1. Camera (Optique)
    auto camera = CreateRef<Camera>(fov, aspect, 0.1f, 1000.0f);
    entity.add<CameraComponent>(camera);
    auto& cc = entity.get<CameraComponent>();
    cc.fov = fov; cc.aspect = aspect; cc.nearPlane = 0.1f; cc.farPlane = 1000.0f;

    // 2. Controller (Logique)
    entity.add<FPSControllerComponent>();

    return View<FPSControllerComponent>(entity);
}

View<ModelComponent> Scene::createModelEntity(const std::string& name, const std::string& path, const glm::vec3& position, const glm::vec3& normalizeSize) {
    if (!m_EngineContext) {
        BB_CORE_ERROR("Scene: Cannot load model '{0}', Engine context is missing!", name);
        return View<ModelComponent>(Entity{}); 
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
        destroyEntity(entity);
        return View<ModelComponent>(Entity{});
    }

    return View<ModelComponent>(entity);
}

View<LightComponent> Scene::createDirectionalLight(const std::string& name, const glm::vec3& color, float intensity, const glm::vec3& rotation) {
    auto entity = createEntity(name);
    entity.add<LightComponent>();
    auto& light = entity.get<LightComponent>();
    light.type = LightType::Directional;
    light.color = color;
    light.intensity = intensity;
    
    entity.get<TransformComponent>().rotation = glm::radians(rotation);
    
    return View<LightComponent>(entity);
}

View<LightComponent> Scene::createPointLight(const std::string& name, const glm::vec3& color, float intensity, float range, const glm::vec3& position) {
    auto entity = createEntity(name);
    entity.at(position);
    
    entity.add<LightComponent>();
    auto& light = entity.get<LightComponent>();
    light.type = LightType::Point;
    light.color = color;
    light.intensity = intensity;
    light.range = range;
    
    return View<LightComponent>(entity);
}

View<SkySphereComponent> Scene::createSkySphere(const std::string& name, const std::string& texturePath) {
    if (!m_EngineContext) {
        BB_CORE_ERROR("Scene: Cannot load skysphere '{0}', Engine context is missing!", name);
        return View<SkySphereComponent>(Entity{});
    }

    auto entity = createEntity(name);
    
    try {
        auto texture = m_EngineContext->assets().load<Texture>(texturePath, true); 
        entity.add<SkySphereComponent>();
        auto& comp = entity.get<SkySphereComponent>();
        comp.texture = texture;
        comp.assetPath = texturePath;
    } catch (const std::exception& e) {
        BB_CORE_ERROR("Scene: Failed to load skysphere texture '{0}': {1}", texturePath, e.what());
        destroyEntity(entity);
        return View<SkySphereComponent>(Entity{});
    }

    return View<SkySphereComponent>(entity);
}

void Scene::destroyEntity(Entity entity) {
    std::string name = "Unknown";
    if (entity.has<TagComponent>()) name = entity.get<TagComponent>().tag;
    
    uint32_t id = (uint32_t)entity.getHandle();
    m_registry.destroy(static_cast<entt::entity>(entity));
    BB_CORE_INFO("Scene: Destroyed entity '{0}' (ID: {1})", name, id);
}

void Scene::onUpdate(float deltaTime) {
    if (!m_EngineContext) return;
    auto& input = m_EngineContext->input();

    // --- SYSTEM: FPS Controller ---
    // Gère le déplacement de la caméra en mode First Person (ZQSD + Souris)
    auto fpsView = m_registry.view<FPSControllerComponent, CameraComponent, TransformComponent>();
    for (auto entity : fpsView) {
        auto [ctrl, cam, trans] = fpsView.get(entity);
        if (!cam.active) continue;

        // Rotation (Souris - Clic Droit maintenu pour "look around")
        if (input.isMouseButtonPressed(Mouse::Right)) {
            glm::vec2 delta = input.getMouseDelta();
            ctrl.yaw += delta.x * ctrl.rotationSpeed.x;
            ctrl.pitch -= delta.y * ctrl.rotationSpeed.y; // Inversé car souris bas = regard haut
            ctrl.pitch = std::clamp(ctrl.pitch, -89.0f, 89.0f); // Empêche le gimbal lock
        }

        // Calcul des vecteurs avant/droite basés sur les angles d'Euler
        glm::vec3 front;
        front.x = cos(glm::radians(ctrl.yaw)) * cos(glm::radians(ctrl.pitch));
        front.y = sin(glm::radians(ctrl.pitch));
        front.z = sin(glm::radians(ctrl.yaw)) * cos(glm::radians(ctrl.pitch));
        glm::vec3 forward = glm::normalize(front);
        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
        glm::vec3 up = glm::normalize(glm::cross(right, forward));

        // Déplacement (Clavier)
        glm::vec3 moveDir(0.0f);
        if (input.isKeyPressed(Key::W)) moveDir += forward * ctrl.movementSpeed.z;
        if (input.isKeyPressed(Key::S)) moveDir -= forward * ctrl.movementSpeed.z;
        if (input.isKeyPressed(Key::D)) moveDir += right * ctrl.movementSpeed.x;
        if (input.isKeyPressed(Key::A)) moveDir -= right * ctrl.movementSpeed.x;
        // Montée/Descente absolue
        if (input.isKeyPressed(Key::Space)) moveDir += glm::vec3(0,1,0) * ctrl.movementSpeed.y;
        if (input.isKeyPressed(Key::LeftShift)) moveDir -= glm::vec3(0,1,0) * ctrl.movementSpeed.y;

        trans.translation += moveDir * deltaTime;

        // Mise à jour de la caméra (View Matrix)
        if (cam.camera) {
            cam.camera->setPosition(trans.translation);
            cam.camera->lookAt(trans.translation + forward);
        }
    }

    // --- SYSTEM: Orbit Controller ---
    // Gère la caméra orbitale (rotation autour d'un point cible)
    auto orbitView = m_registry.view<OrbitControllerComponent, CameraComponent, TransformComponent>();
    for (auto entity : orbitView) {
        auto [ctrl, cam, trans] = orbitView.get(entity);
        if (!cam.active) continue;

        // Rotation (Clic Gauche maintenu)
        if (input.isMouseButtonPressed(Mouse::Left)) {
            glm::vec2 delta = input.getMouseDelta();
            ctrl.yaw += delta.x * ctrl.rotationSpeed.x;
            ctrl.pitch -= delta.y * ctrl.rotationSpeed.y;
            ctrl.pitch = std::clamp(ctrl.pitch, -89.0f, 89.0f);
        }

        // Zoom (Molette)
        float scroll = input.getMouseScroll().y;
        if (scroll != 0.0f) {
            ctrl.distance -= scroll * ctrl.zoomSpeed;
            ctrl.distance = std::clamp(ctrl.distance, ctrl.minDistance, ctrl.maxDistance);
        }

        // Conversion Coordonnées Sphériques -> Cartésiennes
        float x = ctrl.distance * std::cos(glm::radians(ctrl.pitch)) * std::sin(glm::radians(ctrl.yaw));
        float y = ctrl.distance * std::sin(glm::radians(ctrl.pitch));
        float z = ctrl.distance * std::cos(glm::radians(ctrl.pitch)) * std::cos(glm::radians(ctrl.yaw));

        trans.translation = ctrl.target + glm::vec3(x, y, z);

        // Mise à jour de la caméra
        if (cam.camera) {
            cam.camera->setPosition(trans.translation);
            cam.camera->lookAt(ctrl.target);
        }
    }

    // --- SYSTEM: Native Scripts ---
    auto scriptView = m_registry.view<NativeScriptComponent>();
    for (auto entityHandle : scriptView) {
        auto& script = scriptView.get<NativeScriptComponent>(entityHandle);
        if (script.onUpdate) {
            Entity entity(entityHandle, *this);
            script.onUpdate(entity, deltaTime);
        }
    }
}

void Scene::clear() {

    m_registry.clear();

    BB_CORE_INFO("Scene: All entities destroyed.");

}



} // namespace bb3d
