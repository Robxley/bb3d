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
    config.system.logFileName = "unit_test_24.log";
    config.graphics.enableOffscreenRendering = false; // We want to test AUTO-activation
    config.modules.enablePhysics = false;
    config.modules.enableEditor = true;

    Log::Init(config);
    BB_CORE_INFO("--- Test Unitaire 24 : Auto-activation Offscreen pour Post-Process ---");

    auto engine = Engine::Create(config);
    if (!engine) return -1;

    {
        auto scene = engine->CreateScene();
        engine->SetActiveScene(scene);

        // 1. Camera
        scene->createFPSCamera("MainCamera", 45.0f, 1280.0f/720.0f, {0, 2, 8}, engine.get());

        // 2. Assets
        auto houseModelRes = engine->assets().load<Model>("assets/models/house.obj");
        
        auto houseModel = Ref<Model>(houseModelRes);
        houseModel->normalize(glm::vec3(2.0f));
        
        auto houseEntity = scene->createEntity("House");
        houseEntity.add<ModelComponent>(houseModel);

        auto floorMesh = Ref<Mesh>(MeshGenerator::createCheckerboardPlane(engine->graphics(), 20.0f, 20).release());
        scene->createEntity("Floor").add<MeshComponent>(floorMesh);

        // 3. Lighting
        scene->createDirectionalLight("Sun", {1.0f, 1.0f, 1.0f}, 1.5f, {-45, 45, 0});

        BB_CORE_INFO("Starting Engine Run loop (Auto-Offscreen Visual Validation)...");
        engine->Run();

        scene->getRegistry().clear();
        engine->assets().clearCache();
        engine->SetActiveScene(nullptr);
    }

    engine->Shutdown();
    return 0;
}
