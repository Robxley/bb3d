#pragma once

#include "bb3d/core/Base.hpp"
#include <glm/glm.hpp>

namespace bb3d {

    class Scene;
    class Entity;

    struct RaycastResult {
        bool hit = false;
        float fraction = 0.0f;
        glm::vec3 position = { 0.0f, 0.0f, 0.0f };
        glm::vec3 normal = { 0.0f, 0.0f, 0.0f };
        uint32_t bodyID = 0xFFFFFFFF;
    };

    class PhysicsWorld {
    public:
        PhysicsWorld();
        ~PhysicsWorld();

        void init();
        void update(float deltaTime, Scene& scene);
        void shutdown();

        void syncTransforms(Scene& scene);
        void createRigidBody(Entity entity);
        void createCharacterController(Entity entity);

        RaycastResult raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance);

    private:
        struct Impl;
        Scope<Impl> m_impl;
    };

} // namespace bb3d