#include "bb3d/physics/PhysicsWorld.hpp"
#include "bb3d/core/Log.hpp"

namespace bb3d {

    void PhysicsWorld::init() {
        BB_CORE_INFO("PhysicsWorld: Initializing (Stub)...");
        // Todo: Initialize Jolt Physics
        m_initialized = true;
    }

    void PhysicsWorld::update(float) {
        if (!m_initialized) return;
        // Todo: Step simulation
    }

    void PhysicsWorld::shutdown() {
        if (!m_initialized) return;
        BB_CORE_INFO("PhysicsWorld: Shutting down...");
        m_initialized = false;
    }

} // namespace bb3d