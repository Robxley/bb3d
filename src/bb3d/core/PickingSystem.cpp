#include "bb3d/core/PickingSystem.hpp"
#include "bb3d/core/Engine.hpp"
#include "bb3d/core/Log.hpp"
#include "bb3d/scene/Scene.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/physics/PhysicsWorld.hpp"
#include "bb3d/render/Renderer.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <limits>

namespace bb3d {

PickingSystem::PickingSystem(PickingMode mode) : m_mode(mode) {
    BB_CORE_INFO("PickingSystem: Initialized with mode {}", 
        mode == PickingMode::PhysicsRaycast ? "PhysicsRaycast" : 
        mode == PickingMode::ColorPicking ? "ColorPicking" : "None");
}

bool PickingSystem::isSelectable(Entity entity) {
    if (!entity) return false;
    // Opt-out pattern: if no SelectableComponent, entity IS selectable by default
    if (!entity.has<SelectableComponent>()) return true;
    return entity.get<SelectableComponent>().selectable;
}

Entity PickingSystem::pick(glm::vec2 viewportUV, Scene& scene, Engine& engine) {
    if (m_mode == PickingMode::None) return {};

    // Both modes share the same ray-based approach for now
    return pickPhysicsRaycast(viewportUV, scene, engine);
}

Entity PickingSystem::pickPhysicsRaycast(glm::vec2 viewportUV, Scene& scene, Engine& engine) {
    // 1. Find the active camera
    glm::mat4 viewMatrix(1.0f);
    glm::mat4 projMatrix(1.0f);
    bool cameraFound = false;

    auto cameraView = scene.getRegistry().view<CameraComponent>();
    for (auto entity : cameraView) {
        auto& cc = cameraView.get<CameraComponent>(entity);
        if (cc.active && cc.camera) {
            viewMatrix = cc.camera->getViewMatrix();
            projMatrix = cc.camera->getProjectionMatrix();
            cameraFound = true;
            break;
        }
    }

    if (!cameraFound) {
        BB_CORE_WARN("PickingSystem: No active camera found for picking.");
        return {};
    }

    // 2. Unproject viewport UV to world-space ray
    // Convert UV [0,1] to NDC [-1, 1]
    // NOTE: The projection matrix already contains the Vulkan Y-flip (proj[1][1] *= -1)
    // so we do NOT need to manually flip ndc.y here — the inverse projection handles it.
    glm::vec2 ndc = viewportUV * 2.0f - 1.0f;

    glm::mat4 invProj = glm::inverse(projMatrix);
    glm::mat4 invView = glm::inverse(viewMatrix);

    // Near plane point in clip space (Vulkan depth range: [0, 1])
    glm::vec4 clipNear = glm::vec4(ndc.x, ndc.y, 0.0f, 1.0f);
    glm::vec4 clipFar  = glm::vec4(ndc.x, ndc.y, 1.0f, 1.0f);

    // To view space
    glm::vec4 viewNear = invProj * clipNear;
    viewNear /= viewNear.w;
    glm::vec4 viewFar = invProj * clipFar;
    viewFar /= viewFar.w;

    // To world space
    glm::vec3 worldNear = glm::vec3(invView * viewNear);
    glm::vec3 worldFar  = glm::vec3(invView * viewFar);
    glm::vec3 rayDir = glm::normalize(worldFar - worldNear);
    glm::vec3 rayOrigin = worldNear;

    BB_CORE_TRACE("PickingSystem: UV({:.2f},{:.2f}) NDC({:.2f},{:.2f}) Origin({:.1f},{:.1f},{:.1f}) Dir({:.2f},{:.2f},{:.2f})",
        viewportUV.x, viewportUV.y, ndc.x, ndc.y,
        rayOrigin.x, rayOrigin.y, rayOrigin.z,
        rayDir.x, rayDir.y, rayDir.z);

    float maxDistance = 1000.0f;

    // 3. Try physics raycast first (if physics is available)
    auto* physWorld = engine.GetPhysicsWorld();
    if (physWorld) {
        auto result = physWorld->raycast(rayOrigin, rayDir, maxDistance);
        if (result.hit) {
            BB_CORE_INFO("PickingSystem: Physics ray hit! bodyID={} pos({:.1f},{:.1f},{:.1f})", 
                result.bodyID, result.position.x, result.position.y, result.position.z);
            // Find entity by bodyID
            auto physView = scene.getRegistry().view<PhysicsComponent>();
            for (auto entityHandle : physView) {
                auto& phys = physView.get<PhysicsComponent>(entityHandle);
                if (phys.bodyID == result.bodyID) {
                    Entity entity(entityHandle, scene);
                    if (isSelectable(entity)) {
                        return entity;
                    }
                }
            }
        }
    }

    // 4. AABB fallback for entities without PhysicsComponent
    return pickAABBFallback(rayOrigin, rayDir, scene);
}

Entity PickingSystem::pickAABBFallback(glm::vec3 rayOrigin, glm::vec3 rayDir, Scene& scene) {
    Entity closestEntity;
    float closestT = std::numeric_limits<float>::max();

    // Ray-AABB intersection (slab method)
    auto testAABB = [&](const AABB& box) -> float {
        // Handle rays parallel to axis planes (avoid division by zero)
        glm::vec3 invDir;
        for (int i = 0; i < 3; i++) {
            invDir[i] = (std::abs(rayDir[i]) > 1e-8f) ? (1.0f / rayDir[i]) : 1e8f;
        }
        glm::vec3 t1 = (box.min - rayOrigin) * invDir;
        glm::vec3 t2 = (box.max - rayOrigin) * invDir;
        glm::vec3 tMin = glm::min(t1, t2);
        glm::vec3 tMax = glm::max(t1, t2);
        float tNear = glm::max(glm::max(tMin.x, tMin.y), tMin.z);
        float tFar = glm::min(glm::min(tMax.x, tMax.y), tMax.z);
        if (tNear > tFar || tFar < 0.0f) return -1.0f;
        return tNear > 0.0f ? tNear : tFar;
    };

    // Test MeshComponent entities
    auto meshView = scene.getRegistry().view<MeshComponent, TransformComponent>();
    for (auto entityHandle : meshView) {
        Entity entity(entityHandle, scene);
        if (!isSelectable(entity)) continue;
        
        auto& meshComp = meshView.get<MeshComponent>(entityHandle);
        auto& tc = meshView.get<TransformComponent>(entityHandle);
        if (!meshComp.mesh) continue;
        
        AABB worldBox = meshComp.mesh->getBounds().transform(tc.getTransform());
        float t = testAABB(worldBox);
        if (t >= 0.0f && t < closestT) {
            closestT = t;
            closestEntity = entity;
        }
    }

    // Test ModelComponent entities
    auto modelView = scene.getRegistry().view<ModelComponent, TransformComponent>();
    for (auto entityHandle : modelView) {
        Entity entity(entityHandle, scene);
        if (!isSelectable(entity)) continue;
        
        auto& modelComp = modelView.get<ModelComponent>(entityHandle);
        auto& tc = modelView.get<TransformComponent>(entityHandle);
        if (!modelComp.model) continue;
        
        AABB worldBox = modelComp.model->getBounds().transform(tc.getTransform());
        float t = testAABB(worldBox);
        if (t >= 0.0f && t < closestT) {
            closestT = t;
            closestEntity = entity;
        }
    }

    if (closestEntity) {
        BB_CORE_INFO("PickingSystem: AABB hit entity at distance {:.2f}", closestT);
    }
    return closestEntity;
}

} // namespace bb3d
