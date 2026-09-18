#include "bb3d/core/Log.hpp"
#include "bb3d/core/Config.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include <SDL3/SDL.h>
#include <iostream>

int main() {
    bb3d::EngineConfig logConfig;
    logConfig.system.logDirectory = "unit_test_logs";
    logConfig.system.logFileName = "unit_test_02.log";
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

        BB_CORE_INFO("Vulkan initialized successfully.");
        BB_CORE_INFO("Device: {}", context.getDeviceName());

        // Verify negotiated API version is at least Vulkan 1.3
        if (context.getApiVersion() < VK_API_VERSION_1_3) {
            BB_CORE_ERROR("Negotiated Vulkan API version is below 1.3: {:#x}", context.getApiVersion());
            return -1;
        }

        const auto& features = context.getEnabledFeatures();
        if (!features.dynamicRendering || !features.synchronization2 || !features.timelineSemaphore) {
            BB_CORE_ERROR("Required Vulkan features missing (dynamicRendering, synchronization2, timelineSemaphore)");
            return -1;
        }

        BB_CORE_INFO("API Version: {}.{}.{}",
            VK_API_VERSION_MAJOR(context.getApiVersion()),
            VK_API_VERSION_MINOR(context.getApiVersion()),
            VK_API_VERSION_PATCH(context.getApiVersion()));
        BB_CORE_INFO("Vulkan 1.4 Active: {}", context.isVulkan14Supported());
        BB_CORE_INFO("Push Descriptors: {}", features.pushDescriptor);
        BB_CORE_INFO("Dynamic Rendering Local Read: {}", features.dynamicRenderingLocalRead);
        BB_CORE_INFO("Timeline Semaphores: {}", features.timelineSemaphore);
        BB_CORE_INFO("Synchronization2: {}", features.synchronization2);

        // Le destructeur de context appellera cleanup() automatiquement (RAII)
    } catch (const std::exception& e) {
        BB_CORE_ERROR("Erreur fatale : {}", e.what());
        SDL_Quit();
        return -1;
    }

    SDL_Quit();
    return 0;
}