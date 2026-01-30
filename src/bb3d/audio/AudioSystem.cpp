#include "bb3d/audio/AudioSystem.hpp"
#include "bb3d/core/Log.hpp"

namespace bb3d {

    void AudioSystem::init() {
        BB_CORE_INFO("AudioSystem: Initializing (Stub)...");
        // Todo: Initialize Miniaudio / OpenAL
        m_initialized = true;
    }

    void AudioSystem::update(float) {
        if (!m_initialized) return;
        // Todo: Update audio engine
    }

    void AudioSystem::shutdown() {
        if (!m_initialized) return;
        BB_CORE_INFO("AudioSystem: Shutting down...");
        m_initialized = false;
    }

} // namespace bb3d