#include "bb3d/core/Log.hpp"
#include "bb3d/core/JobSystem.hpp"
#include "bb3d/resource/ResourceManager.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/render/Texture.hpp"
#include "bb3d/render/Model.hpp"

#include <iostream>
#include <atomic>
#include <chrono>
#include <fstream>
#include <SDL3/SDL.h>
#include <filesystem>

void createDummyBMP(const std::string& path) {
    std::ofstream f(path, std::ios::binary);
    unsigned char bmpPad[3] = {0,0,0};
    const int w = 1;
    const int h = 1;
    const int filesize = 54 + 3*w*h;  // 1 pixel 24-bit

    unsigned char bmpfileheader[14] = {'B','M', 0,0,0,0, 0,0, 0,0, 54,0,0,0};
    unsigned char bmpinfoheader[40] = {40,0,0,0, 0,0,0,0, 0,0,0,0, 1,0, 24,0};
    unsigned char bmppad[3] = {0,0,0};

    bmpfileheader[ 2] = (unsigned char)(filesize    );
    bmpfileheader[ 3] = (unsigned char)(filesize>> 8);
    bmpfileheader[ 4] = (unsigned char)(filesize>>16);
    bmpfileheader[ 5] = (unsigned char)(filesize>>24);

    bmpinfoheader[ 4] = (unsigned char)(       w    );
    bmpinfoheader[ 5] = (unsigned char)(       w>> 8);
    bmpinfoheader[ 6] = (unsigned char)(       w>>16);
    bmpinfoheader[ 7] = (unsigned char)(       w>>24);
    bmpinfoheader[ 8] = (unsigned char)(       h    );
    bmpinfoheader[ 9] = (unsigned char)(       h>> 8);
    bmpinfoheader[10] = (unsigned char)(       h>>16);
    bmpinfoheader[11] = (unsigned char)(       h>>24);

    f.write((char*)bmpfileheader, 14);
    f.write((char*)bmpinfoheader, 40);
    
    unsigned char pixel[3] = {0, 0, 255}; // Red BGR
    f.write((char*)pixel, 3);
    f.write((char*)bmppad, (4-(w*3)%4)%4);
    f.close();
}

void runResourceManagerTest() {
    BB_CORE_INFO("--- Test Unitaire 09 : Resource Manager Internal ---");
    
    try {
        // Ensure assets exist
        BB_CORE_INFO("[Debug] Creating assets directories...");
        std::filesystem::create_directories("assets/textures");
        std::filesystem::create_directories("assets/models");
        BB_CORE_INFO("[Debug] Creating dummy BMP...");
        createDummyBMP("assets/textures/bricks.bmp");
        
        // Create dummy GLTF if not exists (Box)
        if (!std::filesystem::exists("assets/models/box.gltf")) {
            BB_CORE_INFO("[Debug] Creating dummy GLTF...");
            // ... (keep the gltfContent)
            const std::string gltfContent = R"({
  "asset": { "version": "2.0" },
  "buffers": [
    {
      "byteLength": 44,
      "uri": "data:application/octet-stream;base64,AAABAAIAAAAAAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAAAAAACAPwAAAAA="
    }
  ],
  "bufferViews": [
    {
      "buffer": 0,
      "byteLength": 6,
      "byteOffset": 0,
      "target": 34963
    },
    {
      "buffer": 0,
      "byteLength": 36,
      "byteOffset": 12,
      "target": 34962
    }
  ],
  "accessors": [
    {
      "bufferView": 0,
      "byteOffset": 0,
      "componentType": 5123,
      "count": 6,
      "type": "SCALAR",
      "max": [ 2 ],
      "min": [ 0 ]
    },
    {
      "bufferView": 1,
      "byteOffset": 0,
      "componentType": 5126,
      "count": 3,
      "type": "VEC3",
      "max": [ 1.0, 1.0, 0.0 ],
      "min": [ 0.0, 0.0, 0.0 ]
    }
  ],
  "meshes": [
    {
      "primitives": [
        {
          "attributes": {
            "POSITION": 1
          },
          "indices": 0
        }
      ]
    }
  ],
  "nodes": [
    {
      "mesh": 0
    }
  ],
  "scenes": [
    {
      "nodes": [ 0 ]
    }
  ],
  "scene": 0
})";
            std::ofstream("assets/models/box.gltf") << gltfContent;
        }

        BB_CORE_INFO("[Debug] Initializing JobSystem...");
        bb3d::JobSystem jobSystem;
        jobSystem.init(4);
        
        BB_CORE_INFO("[Debug] SDL_Init...");
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            BB_CORE_ERROR("SDL Init failed: {}", SDL_GetError());
            return;
        }

        BB_CORE_INFO("[Debug] Initializing VulkanContext...");
        bb3d::VulkanContext context;
        context.init(nullptr, "ResourceTest", false);

        BB_CORE_INFO("[Debug] Creating ResourceManager...");
        bb3d::ResourceManager assets(context, jobSystem);

        // 1. Test Caching Synchrone
        {
            BB_CORE_INFO("[Test] Caching...");
            auto tex1 = assets.load<bb3d::Texture>("assets/textures/bricks.bmp");
            auto tex2 = assets.load<bb3d::Texture>("assets/textures/bricks.bmp");

            if (tex1 && tex2 && tex1 == tex2) {
                BB_CORE_INFO("[Success] Le cache fonctionne : tex1 == tex2");
            } else {
                BB_CORE_ERROR("[Fail] Le cache ne fonctionne pas ou texture nulle.");
                throw std::runtime_error("Caching failed");
            }
        }

        // 2. Test Asynchrone
        {
            BB_CORE_INFO("[Test] Chargement Asynchrone...");
            std::atomic<bool> loaded{false};
            bb3d::Ref<bb3d::Model> loadedModel = nullptr;

            assets.loadAsync<bb3d::Model>("assets/models/box.gltf", [&](bb3d::Ref<bb3d::Model> model) {
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

            if (loaded && loadedModel != nullptr && loadedModel->getPath() == "assets/models/box.gltf") {
                BB_CORE_INFO("[Success] Chargement asynchrone réussi.");
            } else {
                BB_CORE_ERROR("[Fail] Le chargement asynchrone a échoué. (Loaded: {}, Model: {})", loaded.load(), (void*)loadedModel.get());
                throw std::runtime_error("Async load failed");
            }
        }

    } catch (const std::exception& e) {
        BB_CORE_ERROR("Erreur pendant le test : {}", e.what());
        throw; // Re-throw to fail the test
    }

    SDL_Quit();
    BB_CORE_INFO("Tests Resource Manager terminés.");
}

int main() {
    try {
        bb3d::EngineConfig logConfig;
        logConfig.system.logDirectory = "unit_test_logs";
        bb3d::Log::Init(logConfig);
        BB_CORE_INFO("--- Test Unitaire 09 : Resource Manager ---");
        runResourceManagerTest();
    } catch (...) {
        return 1;
    }
    return 0;
}