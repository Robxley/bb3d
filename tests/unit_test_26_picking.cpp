// unit_test_26_picking.cpp
// Validates GPU Color Picking robustness fixes (B4, B5, B6):
// 1. Eager picking initialization (B5): hasPickingBuffer() is ready without lazy mid-frame creation.
// 2. Out-of-bounds and sentinel readback (Q15): returns kPickingNoEntity without crashing.
// 3. Repeated resize stability (B4): verifies no descriptor set leaks / pool exhaustion across multiple resizes.
// 4. Isolated non-blocking fence readback (B6).

#include "bb3d/core/Engine.hpp"
#include "bb3d/core/Log.hpp"
#include "bb3d/scene/Scene.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/render/Renderer.hpp"
#include "bb3d/render/MeshGenerator.hpp"
#include "bb3d/core/PickingSystem.hpp"
#include <cassert>
#include <iostream>

using namespace bb3d;

int main() {
    EngineConfig config;
    config.system.logDirectory = "unit_test_logs";
    config.system.logFileName = "unit_test_26.log";
    config.graphics.enableOffscreenRendering = true;
    config.modules.enablePhysics = false;
    config.modules.enableEditor = false;

    Log::Init(config);
    BB_CORE_INFO("--- Test Unitaire 26 : GPU Color Picking Robustness (B4, B5, B6) ---");

    auto engine = Engine::Create(config);
    if (!engine) {
        std::cerr << "Failed to create engine instance" << std::endl;
        return -1;
    }

    {
        auto& renderer = engine->GetRenderer();

        // Checkpoint 1 (B5 fix): Picking buffer must be ready eagerly upon Renderer creation
        assert(renderer.hasPickingBuffer() && "Picking buffer must be ready after engine initialization");

        // Checkpoint 2 (Q15): Sentinel constant check
        assert(Renderer::kPickingNoEntity == 0xFFFFFFFF);

        // Checkpoint 3 (B6 readback): Reading before render or out of bounds returns kPickingNoEntity
        uint32_t oobId = renderer.readEntityIdAt(99999, 99999);
        assert(oobId == Renderer::kPickingNoEntity && "Out of bounds picking read must return kPickingNoEntity");

        auto scene = engine->CreateScene();
        engine->SetActiveScene(scene);
        scene->createFPSCamera("TestCam", 45.0f, 16.0f / 9.0f, {0, 0, 5}, engine.get());

        // Checkpoint 4 (B4 fix): Multiple resize cycles must not leak descriptor sets or exhaust the pool
        constexpr int kResizeCycles = 10;
        for (int i = 0; i < kResizeCycles; ++i) {
            int w = 640 + (i * 32);
            int h = 480 + (i * 24);
            renderer.onResize(w, h);
            renderer.requestPicking();
            bool rendered = renderer.render(*scene);
            if (rendered) {
                renderer.submitAndPresent();
            }
            assert(renderer.hasPickingBuffer() && "Picking buffer must stay valid after resize");
            // Check that readEntityIdAt executes cleanly with the fence-based path
            uint32_t sampleId = renderer.readEntityIdAt(10, 10);
            (void)sampleId;
        }

        // Checkpoint 5 (Editor Viewport sync & 1:1 Color Picking):
        // Simulate editor docking / viewport resize by resizing the RenderTarget directly.
        auto rt = renderer.getRenderTarget();
        assert(rt != nullptr && "Offscreen render target must exist");
        rt->resize(800, 600);

        // Position camera looking directly at origin
        auto camView = scene->getRegistry().view<CameraComponent>();
        for (auto entity : camView) {
            auto& cc = camView.get<CameraComponent>(entity);
            if (cc.active && cc.camera) {
                cc.camera->setPosition({0.0f, 0.0f, 5.0f});
                cc.camera->lookAt({0.0f, 0.0f, 0.0f});
                cc.camera->setAspectRatio(800.0f / 600.0f);
            }
        }

        // Add a mesh cube in the center of the camera view
        auto cubeMesh = Ref<Mesh>(MeshGenerator::createCube(engine->graphics(), 1.0f, {1, 0, 0}).release());
        Entity redCube = scene->createEntity("RedCube")
            .at({0.0f, 0.0f, 0.0f})
            .add<MeshComponent>(cubeMesh, "", PrimitiveType::Cube);

        // Switch to ColorPicking mode
        auto* pickingSys = engine->GetPickingSystem();
        assert(pickingSys != nullptr);
        pickingSys->setMode(PickingMode::ColorPicking);

        // Render frame with picking request
        renderer.requestPicking();
        bool rendered = renderer.render(*scene);
        assert(rendered && "Render must succeed");
        renderer.submitAndPresent();

        // Verify that picking buffer dimensions are synchronized to RenderTarget (800x600)
        assert(renderer.getPickingWidth() == 800 && "Picking width must match RenderTarget after render()");
        assert(renderer.getPickingHeight() == 600 && "Picking height must match RenderTarget after render()");

        // Pick at the center of the viewport (UV: 0.5, 0.5) -> should hit RedCube!
        uint32_t rawId = renderer.readEntityIdAt(400, 300);
        std::cout << ">>> getPickingWidth=" << renderer.getPickingWidth() 
                  << " getPickingHeight=" << renderer.getPickingHeight() 
                  << " rawId(400, 300)=" << rawId 
                  << " redCubeHandle=" << (uint32_t)redCube.getHandle() << std::endl;

        for (uint32_t x = 0; x < 800; x += 50) {
            uint32_t id = renderer.readEntityIdAt(x, 300);
            if (id != Renderer::kPickingNoEntity) {
                std::cout << ">>> Found entity " << id << " at (" << x << ", 300)" << std::endl;
            }
        }

        Entity pickedCenter = scene->pickEntity({0.5f, 0.5f});
        assert(pickedCenter && "Color picking at center (0.5, 0.5) must hit the RedCube!");
        assert(pickedCenter == redCube && "Hit entity must be RedCube!");

        // Pick at the corner (UV: 0.05, 0.05) -> should miss!
        Entity pickedCorner = scene->pickEntity({0.05f, 0.05f});
        assert(!pickedCorner && "Color picking at corner (0.05, 0.05) must not hit any entity!");

        scene->getRegistry().clear();
        engine->SetActiveScene(nullptr);
    }

    engine->Shutdown();
    BB_CORE_INFO("--- Test Unitaire 26 : Succès ! ---");
    return 0;
}
