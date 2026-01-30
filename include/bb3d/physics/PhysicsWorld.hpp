#pragma once

#include "bb3d/core/Base.hpp"

namespace bb3d {

    class PhysicsWorld {
    public:
        PhysicsWorld() = default;
        ~PhysicsWorld() = default;

        void init();
        void update(float deltaTime);
        void shutdown();

    private:
        bool m_initialized = false;
    };

} // namespace bb3d