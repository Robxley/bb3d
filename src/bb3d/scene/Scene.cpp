#include "bb3d/scene/Scene.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/scene/Camera.hpp"
#include "bb3d/core/Engine.hpp"
#include "bb3d/physics/PhysicsWorld.hpp"
#include "bb3d/render/ProceduralMeshGenerator.hpp"
#include "bb3d/render/Material.hpp"
#include "bb3d/core/PickingSystem.hpp"
#include <algorithm>
#include <glm/gtc/constants.hpp>

namespace bb3d {


Entity Scene::findEntityByName(const std::string& name) {
    auto it = m_EntityNames.find(name);
    if (it != m_EntityNames.end()) {
        if (m_registry.valid(it->second)) {
            return { it->second, *this };
        } else {
            // Lazy cleanup if the entry is stale (though destroyEntity should handle it)
            m_EntityNames.erase(it);
        }
    }
    return {};
}

Entity Scene::createEntity(const std::string& name) {
    Entity entity(m_registry.create(), *this);
    if (!name.empty()) {
        entity.add<TagComponent>(name);
        m_EntityNames[name] = entity;
    }
    // Every entity has at least one default transform
    entity.add<TransformComponent>();
    
    BB_CORE_INFO("Scene: Created entity '{0}' (ID: {1})", name.empty() ? "Unnamed" : name, (uint32_t)entity.getHandle());
    return entity;
}

View<OrbitControllerComponent> Scene::createOrbitCamera(const std::string& name, float fov, float aspect, const glm::vec3& target, float distance, Engine* /*engine*/) {
    auto entity = createEntity(name);
    
    // 1. Camera (Optique)
    auto camera = CreateRef<Camera>(fov, aspect, 0.2f, 400.0f);
    entity.add<CameraComponent>(camera);
    auto& cc = entity.get<CameraComponent>();
    cc.fov = fov; cc.aspect = aspect; cc.nearPlane = 0.2f; cc.farPlane = 400.0f;

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
    auto camera = CreateRef<Camera>(fov, aspect, 0.2f, 400.0f);
    entity.add<CameraComponent>(camera);
    auto& cc = entity.get<CameraComponent>();
    cc.fov = fov; cc.aspect = aspect; cc.nearPlane = 0.2f; cc.farPlane = 400.0f;

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
    light.castShadows = true; // Shadows are on by default for the Sun
    
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

View<SkySphereComponent> Scene::createSkySphere(const std::string& name, const std::string& texturePath, bool flipY) {
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
        comp.flipY = flipY;
    } catch (const std::exception& e) {
        BB_CORE_ERROR("Scene: Failed to load skysphere texture '{0}': {1}", texturePath, e.what());
        destroyEntity(entity);
        return View<SkySphereComponent>(Entity{});
    }

    return View<SkySphereComponent>(entity);
}

void Scene::destroyEntity(Entity entity) {
    std::string name = "Unknown";
    if (entity.has<TagComponent>()) {
        name = entity.get<TagComponent>().tag;
        m_EntityNames.erase(name);
    }
    
    uint32_t id = (uint32_t)entity.getHandle();
    entt::entity handle = static_cast<entt::entity>(entity);
    m_WarnedEntities.erase(handle);
    m_registry.destroy(handle);
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

        // Rotation (Mouse - Right Click held to look around)
        if (input.isMouseButtonPressed(Mouse::Right)) {
            glm::vec2 delta = input.getMouseDelta();
            ctrl.yaw += delta.x * ctrl.rotationSpeed.x;
            ctrl.pitch -= delta.y * ctrl.rotationSpeed.y; // Inverted because mouse down = look up
            ctrl.pitch = std::clamp(ctrl.pitch, -89.0f, 89.0f); // Prevent gimbal lock
        }

        // Calculate forward/right vectors based on Euler angles
        glm::vec3 front;
        front.x = cos(glm::radians(ctrl.yaw)) * cos(glm::radians(ctrl.pitch));
        front.y = sin(glm::radians(ctrl.pitch));
        front.z = sin(glm::radians(ctrl.yaw)) * cos(glm::radians(ctrl.pitch));
        glm::vec3 forward = glm::normalize(front);
        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
        glm::vec3 up = glm::normalize(glm::cross(right, forward));

        // Movement (Keyboard)
        glm::vec3 moveDir(0.0f);
        if (input.isKeyPressed(Key::W)) moveDir += forward * ctrl.movementSpeed.z;
        if (input.isKeyPressed(Key::S)) moveDir -= forward * ctrl.movementSpeed.z;
        if (input.isKeyPressed(Key::D)) moveDir += right * ctrl.movementSpeed.x;
        if (input.isKeyPressed(Key::A)) moveDir -= right * ctrl.movementSpeed.x;
        // Absolute vertical movement (Rise/Fall)
        if (input.isKeyPressed(Key::Space)) moveDir += glm::vec3(0,1,0) * ctrl.movementSpeed.y;
        if (input.isKeyPressed(Key::LeftShift)) moveDir -= glm::vec3(0,1,0) * ctrl.movementSpeed.y;

        trans.translation += moveDir * deltaTime;

        // Update camera (View Matrix)
        if (cam.camera) {
            cam.camera->setPosition(trans.translation);
            cam.camera->lookAt(trans.translation + forward);
        }
    }

    // --- SYSTEM: Orbit Controller ---
    // Manages orbital camera (rotation around a focus point)
    auto orbitView = m_registry.view<OrbitControllerComponent, CameraComponent, TransformComponent>();
    for (auto entity : orbitView) {
        auto [ctrl, cam, trans] = orbitView.get(entity);
        if (!cam.active) continue;

        // Rotation (Left Click held)
        if (input.isMouseButtonPressed(Mouse::Left)) {
            glm::vec2 delta = input.getMouseDelta();
            ctrl.yaw += delta.x * ctrl.rotationSpeed.x;
            ctrl.pitch -= delta.y * ctrl.rotationSpeed.y;
            ctrl.pitch = std::clamp(ctrl.pitch, -89.0f, 89.0f);
        }

        // Zoom (Wheel)
        float scroll = input.getMouseScroll().y;
        if (scroll != 0.0f) {
            ctrl.distance -= scroll * ctrl.zoomSpeed;
            ctrl.distance = std::clamp(ctrl.distance, ctrl.minDistance, ctrl.maxDistance);
        }

        // Spherical Coordinates -> Cartesian conversion
        float x = ctrl.distance * std::cos(glm::radians(ctrl.pitch)) * std::sin(glm::radians(ctrl.yaw));
        float y = ctrl.distance * std::sin(glm::radians(ctrl.pitch));
        float z = ctrl.distance * std::cos(glm::radians(ctrl.pitch)) * std::cos(glm::radians(ctrl.yaw));

        trans.translation = ctrl.target + glm::vec3(x, y, z);

        // Update camera
        if (cam.camera) {
            cam.camera->setPosition(trans.translation);
            cam.camera->lookAt(ctrl.target);
        }
    }

    // --- SYSTEM: Simple Animations ---
    auto animView = m_registry.view<SimpleAnimationComponent, TransformComponent>();
    for (auto entityHandle : animView) {
        auto [anim, trans] = animView.get(entityHandle);
        if (!anim.active) continue;

        anim.timeAccumulator += deltaTime;
        Entity entity(entityHandle, *this);

        if (!anim.initialized) {
            trans.initialTranslation = trans.translation;
            trans.initialRotation = trans.rotation;
        }

        bool transformChanged = false;

        if (anim.type == SimpleAnimationType::Rotation) {
            // Local Space: Stable continuous rotation
            float dAngle = anim.speed * deltaTime;
            trans.rotation += anim.rotationAxis * dAngle;
            trans.rotation.x = glm::mod(trans.rotation.x, glm::two_pi<float>());
            trans.rotation.y = glm::mod(trans.rotation.y, glm::two_pi<float>());
            trans.rotation.z = glm::mod(trans.rotation.z, glm::two_pi<float>());
            transformChanged = true;
        }
        else if (anim.type == SimpleAnimationType::Translation) {
            // Local Space: Oscillation relative to initial position
            float phase = sin(anim.timeAccumulator * anim.speed);
            trans.translation = trans.initialTranslation + (anim.direction * phase * anim.amplitude);
            transformChanged = true;
        }
        else if (anim.type == SimpleAnimationType::Waypoints && !anim.waypoints.empty()) {
            // World Space: Move towards waypoints
            size_t nextIdx = (anim.currentWaypoint + 1) % anim.waypoints.size();
            glm::vec3 start = anim.waypoints[anim.currentWaypoint];
            glm::vec3 end = anim.waypoints[nextIdx];
            
            float dist = glm::distance(trans.translation, end);
            if (dist < 0.1f) {
                anim.currentWaypoint = (int)nextIdx;
                // If not looping, stop at the end of the chain
                if (!anim.loop && anim.currentWaypoint == 0) {
                    anim.active = false;
                }
            } else {
                glm::vec3 dir = glm::normalize(end - start);
                trans.translation += dir * anim.speed * deltaTime;
                transformChanged = true;
            }
        }

        if (transformChanged && anim.physicsSync && entity.has<PhysicsComponent>()) {
            auto& phys = entity.get<PhysicsComponent>();
            // Force Kinematic by default if animating to ensure collision works correctly (pushing player)
            if (!anim.initialized) {
                if (phys.type != BodyType::Kinematic) {
                    phys.type = BodyType::Kinematic;
                    m_EngineContext->physics().resetBody(entity);
                }
                anim.initialized = true;
            }
            m_EngineContext->physics().updateBodyTransform(entity);
        }
    }

    // --- SYSTEM: Procedural Planets ---
    auto planetView = m_registry.view<ProceduralPlanetComponent>();
    for (auto entityHandle : planetView) {
        Entity entity(entityHandle, *this);
        auto& planet = planetView.get<ProceduralPlanetComponent>(entityHandle);
        if (planet.needsRebuild) {
             BB_CORE_INFO("Scene: Rebuilding procedural planet for entity '{}' (Resolution: {}, Radius: {})...", 
                entity.getName(), planet.resolution, planet.radius);
                
            planet.model = ProceduralMeshGenerator::createPlanet(m_EngineContext->graphics(), m_EngineContext->assets(), m_EngineContext->jobs(), planet);
            
            if (planet.model) {
                BB_CORE_INFO("Scene: Planet rebuild complete for '{}' ({} meshes generated)", 
                    entity.getName(), planet.model->getMeshes().size());
            } else {
                BB_CORE_ERROR("Scene: Planet rebuild FAILED for entity '{}'!", entity.getName());
            }

            // Re-apply PBR material if none exists
            if (planet.model) {
                for (auto& mesh : planet.model->getMeshes()) {
                    if (!mesh->getMaterial()) {
                        auto pbr = CreateRef<PBRMaterial>(m_EngineContext->graphics());
                        pbr->setColor({1.0f, 1.0f, 1.0f}); // Use vertex colors
                        mesh->setMaterial(pbr);
                    }
                }
            }
            
            planet.needsRebuild = false;

            // Link to a standard ModelComponent to simplify rendering and culling
            if (!entity.has<ModelComponent>()) {
                entity.add<ModelComponent>(planet.model);
            } else {
                entity.get<ModelComponent>().model = planet.model;
            }

            // If it has physics, we must rebuild the rigid body to match the new mesh
            if (entity.has<PhysicsComponent>()) {
                m_EngineContext->physics().destroyRigidBody(entity);
                m_EngineContext->physics().createRigidBody(entity);
            }
        }

        // --- OPTIMIZATION: Horizon Culling ---
        if (planet.model) {
            entt::entity activeCamera = entt::null;
            auto camView = m_registry.view<CameraComponent>();
            for (auto camEnt : camView) {
                if (camView.get<CameraComponent>(camEnt).active) {
                    activeCamera = camEnt;
                    break;
                }
            }

            if (activeCamera != entt::null) {
                auto& camTransform = m_registry.get<TransformComponent>(activeCamera);
                glm::vec3 camPos = camTransform.translation;
                
                auto& planetTransform = entity.get<TransformComponent>();
                glm::vec3 planetPos = planetTransform.translation;
                glm::mat4 planetRotation = glm::toMat4(glm::quat(planetTransform.rotation));
                
                // The 6 directions representing the faces of the cube (must match ProceduralMeshGenerator)
                static const std::vector<glm::vec3> faceDirections = {
                    { 1.0f,  0.0f,  0.0f}, {-1.0f,  0.0f,  0.0f},
                    { 0.0f,  1.0f,  0.0f}, { 0.0f, -1.0f,  0.0f},
                    { 0.0f,  0.0f,  1.0f}, { 0.0f,  0.0f, -1.0f}
                };

/*
                const auto& meshes = planet.model->getMeshes();
                for (size_t i = 0; i < meshes.size() && i < faceDirections.size(); ++i) {
                    glm::vec3 worldNormal = glm::normalize(glm::vec3(planetRotation * glm::vec4(faceDirections[i], 0.0f)));
                    glm::vec3 toPlanet = glm::normalize(planetPos - camPos);
                    // Simple Horizon Culling: If the face normal is too far from the camera direction (pointing away)
                    // We use 0.55f (approx sin(33 deg)) to account for the angular span of a cube face over the sphere.
                    float dot = glm::dot(worldNormal, toPlanet);
                    meshes[i]->setVisible(dot < 0.55f);
                }
*/
            }
        }
    }

    // --- SYSTEM: Particles ---
    auto particleView = m_registry.view<ParticleSystemComponent>();
    for (auto entityHandle : particleView) {
        auto& particleSys = particleView.get<ParticleSystemComponent>(entityHandle);
        for (auto& p : particleSys.particlePool) {
            if (p.lifeRemaining <= 0.0f) continue;
            
            p.lifeRemaining -= deltaTime;
            if (p.lifeRemaining > 0.0f) {
                p.position += p.velocity * deltaTime;
                
                // Note: Jolt Physics integration for particles could go here (if particleSys.injectIntoPhysics).
            }
        }
    }

    // --- SYSTEM: Native Scripts ---
    auto scriptView = m_registry.view<NativeScriptComponent>();
    for (auto entityHandle : scriptView) {
        auto& script = scriptView.get<NativeScriptComponent>(entityHandle);
        if (script.onUpdate) {
            Entity entity(entityHandle, *this);
            script.onUpdate(entity, deltaTime);
        } else if (!script.name.empty()) {
            if (!m_WarnedEntities.contains(entityHandle)) {
                BB_CORE_WARN("Scene: NativeScriptComponent on entity '{}' has name '{}' but no onUpdate lambda! Did you forget to bind it?", 
                    Entity(entityHandle, *this).getName(), script.name);
                m_WarnedEntities.insert(entityHandle);
            }
        }
    }
}

void Scene::clear() {
    if (m_EngineContext && m_EngineContext->GetPhysicsWorld()) {
        m_EngineContext->physics().clear();
    }

    m_registry.clear();
    m_EntityNames.clear();
    m_WarnedEntities.clear();
    BB_CORE_INFO("Scene: All entities destroyed.");
}


Entity Scene::pickEntity(glm::vec2 viewportUV) {
    if (!m_EngineContext) return {};
    auto* pickingSys = m_EngineContext->GetPickingSystem();
    if (!pickingSys) return {};
    return pickingSys->pick(viewportUV, *this, *m_EngineContext);
}

} // namespace bb3d
