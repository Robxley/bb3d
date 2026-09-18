#include "bb3d/core/Engine.hpp"
#include "bb3d/core/Log.hpp"
#include "bb3d/scene/Scene.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/scene/OrbitCamera.hpp"
#include "bb3d/render/MeshGenerator.hpp"
#include "bb3d/render/Material.hpp"
#include <iostream>

using namespace bb3d;

int main() {
    EngineConfig config;
    config.system.logDirectory = "unit_test_logs";
    config.system.logFileName = "unit_test_23.log";
    config.graphics.enableOffscreenRendering = true; 
    config.graphics.enableTonemapping = true;
    config.graphics.exposure = 1.2f;
    config.modules.enablePhysics = false;
    config.modules.enableEditor = true; // Re-enable editor for manual scene manipulation

    Log::Init(config);
    BB_CORE_INFO("--- Test Unitaire 23 : Post-Processing ---");

    auto engine = Engine::Create(config);
    if (!engine) return -1;

    {
        auto scene = engine->CreateScene();
        engine->SetActiveScene(scene);

        // 1. Camera & Interaction
        auto cameraEntity = scene->createEntity("MainCamera");
        auto orbitCam = CreateRef<OrbitCamera>(45.0f, 1280.0f/720.0f, 0.1f, 100.0f);
        orbitCam->setTarget({0, 1, 0});
        orbitCam->zoom(-10.0f);
        cameraEntity.add<CameraComponent>(orbitCam);

        cameraEntity.add<NativeScriptComponent>([eng = engine.get()](Entity entity, float dt) {
            auto& camComp = entity.get<CameraComponent>();
            auto* orbit = dynamic_cast<OrbitCamera*>(camComp.camera.get());
            if (!orbit) return;

            auto& input = eng->input();
            if (input.isMouseButtonPressed(Mouse::Left)) {
                glm::vec2 delta = input.getMouseDelta();
                orbit->rotate(delta.x * 5.0f, -delta.y * 5.0f);
            }
            orbit->zoom(input.getMouseScroll().y);
        });

        // 2. Environment (SkySphere)
        scene->createSkySphere("Environment", "assets/textures/skybox_sphere_wood_diffuse.jpeg", false);

        // 3. Visual Assets
        auto sphereMeshRes = MeshGenerator::createSphere(engine->graphics(), 0.5f, 32);
        auto floorMeshRes = MeshGenerator::createCheckerboardPlane(engine->graphics(), 20.0f, 20);
        
        auto sphereMesh = Ref<Mesh>(sphereMeshRes.release());
        auto floorMesh = Ref<Mesh>(floorMeshRes.release());

        scene->createEntity("Floor").add<MeshComponent>(floorMesh);

        // Grid of colorful spheres with PBR Materials
        for (int x = -2; x <= 2; ++x) {
            for (int z = -2; z <= 2; ++z) {
                auto e = scene->createEntity("Sphere_" + std::to_string(x) + "_" + std::to_string(z));
                e.at({(float)x * 1.5f, 1.0f, (float)z * 1.5f});
                
                // Each mesh must have its own unique material instance for different colors
                auto instanceMesh = Ref<Mesh>(MeshGenerator::createSphere(engine->graphics(), 0.5f, 32).release());
                auto mat = CreateRef<PBRMaterial>(engine->graphics());
                
                PBRParameters params;
                params.baseColorFactor = { (x + 2) / 4.0f, (z + 2) / 4.0f, 0.5f, 1.0f };
                params.roughnessFactor = 0.1f + (z + 2) / 10.0f;
                params.metallicFactor = (x + 2) / 4.0f;
                mat->setParameters(params);
                
                instanceMesh->setMaterial(mat);
                e.add<MeshComponent>(instanceMesh);
            }
        }

        // 4. Lighting
        scene->createDirectionalLight("Sun", {1.0f, 0.9f, 0.8f}, 3.0f, {-45, 45, 0});

        // 5. Runtime Test Logic (Cycle parameters every few seconds)
        auto controller = scene->createEntity("Controller");
        controller.add<NativeScriptComponent>([&](Entity e, float dt) {
            static float timer = 0.0f;
            timer += dt;

            auto& liveConfig = const_cast<EngineConfig&>(engine->GetConfig());
            
            // Cycle exposure every 5 seconds for visual comparison
            if (fmod(timer, 10.0f) < 5.0f) {
                liveConfig.graphics.exposure = 0.8f;
            } else {
                liveConfig.graphics.exposure = 2.5f; // Very bright, needs tonemapping
            }
        });

        BB_CORE_INFO("Starting Engine Run loop for visual validation...");
        engine->Run();

        scene->getRegistry().clear();
        scene->getRegistry().clear();
        engine->assets().clearCache();
        engine->SetActiveScene(nullptr);
    }

    engine->Shutdown();
    return 0;
}
