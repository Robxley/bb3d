#include "bb3d/core/Log.hpp"
#include "bb3d/core/JobSystem.hpp"
#include "bb3d/resource/ResourceManager.hpp"
#include "bb3d/render/VulkanContext.hpp"

#include <iostream>
#include <atomic>
#include <chrono>
#include <SDL3/SDL.h>

void runResourceManagerTest() {
    bb3d::Log::Init();
    BB_CORE_INFO("--- Test Unitaire 08 : Resource Manager ---");

    bb3d::JobSystem jobSystem;
    jobSystem.init(4);
    
    // Initialisation SDL requise par VulkanContext
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        BB_CORE_ERROR("SDL Init failed");
        return;
    }

    try {
        bb3d::VulkanContext context;
        // Init minimaliste pour le test
        context.init(nullptr, "ResourceTest", false);

        bb3d::ResourceManager assets(context, jobSystem);

        // 1. Test Caching Synchrone
        {
            BB_CORE_INFO("[Test] Caching...");
            auto tex1 = assets.load<bb3d::Texture>("textures/bricks.png");
            auto tex2 = assets.load<bb3d::Texture>("textures/bricks.png");

            if (tex1 == tex2) {
                BB_CORE_INFO("[Success] Le cache fonctionne : tex1 == tex2");
            } else {
                BB_CORE_ERROR("[Fail] Le cache ne fonctionne pas : tex1 != tex2");
                throw std::runtime_error("Caching failed");
            }
        }

        // 2. Test Asynchrone
        {
            BB_CORE_INFO("[Test] Chargement Asynchrone...");
            std::atomic<bool> loaded{false};
            bb3d::Ref<bb3d::Model> loadedModel = nullptr;

            assets.loadAsync<bb3d::Model>("models/car.glb", [&](bb3d::Ref<bb3d::Model> model) {
                BB_CORE_INFO("Callback: Modèle chargé asynchronement.");
                loadedModel = model;
                loaded = true;
            });

            // Attente active courte
            int attempts = 0;
            while (!loaded && attempts < 100) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                attempts++;
            }

            if (loaded && loadedModel != nullptr && loadedModel->getPath() == "models/car.glb") {
                BB_CORE_INFO("[Success] Chargement asynchrone réussi.");
            } else {
                BB_CORE_ERROR("[Fail] Le chargement asynchrone a échoué.");
                throw std::runtime_error("Async load failed");
            }
        }

    } catch (const std::exception& e) {
        BB_CORE_ERROR("Erreur pendant le test : {}", e.what());
    }

    SDL_Quit();
    BB_CORE_INFO("Tests Resource Manager terminés.");
}

int main() {
    runResourceManagerTest();
    return 0;
}