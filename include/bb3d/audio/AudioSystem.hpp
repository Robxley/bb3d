#pragma once

#include "bb3d/core/Base.hpp"

namespace bb3d {

    class AudioSystem {
    public:
        AudioSystem() = default;
        ~AudioSystem() = default;

        void init();
        void update(float deltaTime);
        void shutdown();

    private:
        bool m_initialized = false;
    };

} // namespace bb3d