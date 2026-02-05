#pragma once

#include "bb3d/core/Base.hpp"

namespace bb3d {

    /**
     * @brief Gère l'émission et la spatialisation sonore.
     * 
     * Basé sur le principe de "AudioSource" (émetteur) et "AudioListener" (récepteur).
     * Utilise Miniaudio ou OpenAL Soft en arrière-plan.
     */
    class AudioSystem {
    public:
        AudioSystem() = default;
        ~AudioSystem() = default;

        /** @brief Initialise le moteur audio et le périphérique de sortie par défaut. */
        void init();
        
        /** @brief Met à jour la position des émetteurs et récepteurs pour le son 3D. */
        void update(float deltaTime);

        /** @brief Arrête le système audio. */
        void shutdown();

    private:
        bool m_initialized = false;
    };

} // namespace bb3d