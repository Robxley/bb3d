#include "bb3d/core/Log.hpp"
#include "bb3d/core/Config.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include <SDL3/SDL.h>
#include <iostream>

int main() {
    bb3d::EngineConfig logConfig;
    logConfig.system.logDirectory = "unit_test_logs";
    bb3d::Log::Init(logConfig);
    BB_CORE_INFO("Test Unitaire 02 : Initialisation Vulkan");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        BB_CORE_ERROR("Échec de l'initialisation de SDL : {}", SDL_GetError());
        return -1;
    }

    try {
        bb3d::VulkanContext context;
        
        // Initialisation (Active les layers de validation en Debug)
#ifdef NDEBUG
        bool validation = false;
#else
        bool validation = true;
#endif
        // Pas de fenêtre pour ce test basique d'init Vulkan
        context.init(nullptr, "Unit Test Vulkan", validation);

        BB_CORE_INFO("Vulkan initialisé avec succès.");
        BB_CORE_INFO("Device: {}", context.getDeviceName());

        // Le destructeur de context appellera cleanup() automatiquement (RAII)
    } catch (const std::exception& e) {
        BB_CORE_ERROR("Erreur fatale : {}", e.what());
        SDL_Quit();
        return -1;
    }

    SDL_Quit();
    return 0;
}