#include "bb3d/core/Engine.hpp"
#include "bb3d/core/Log.hpp"
#include "bb3d/core/Config.hpp"
#include "bb3d/render/Renderer.hpp"
#include "bb3d/render/MeshGenerator.hpp"
#include "bb3d/scene/Scene.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/scene/Camera.hpp"

#include <cassert>
#include <vector>
#include <algorithm>
#include <iostream>

// Verify compile-time requirements for DOD / L1 cache line performance (Rule 0)
static_assert(sizeof(bb3d::RenderCommand) == 24, "RenderCommand must be exactly 24 bytes for optimal L1 cache line occupancy!");
static_assert(alignof(bb3d::RenderCommand) == alignof(void*), "RenderCommand must be aligned to pointer size!");

int main() {
    bb3d::EngineConfig config;
    config.system.logDirectory = "unit_test_logs";
    config.system.logFileName = "unit_test_36_renderer_hotpath_optim.log";
    config.window.title = "Unit Test Hot-Path Optim";
    config.window.width = 640;
    config.window.height = 480;
    config.graphics.enableOffscreenRendering = false;
    config.graphics.enableFrustumCulling = true;
    config.graphics.shadowsEnabled = true;
    config.modules.enablePhysics = false;
    config.modules.enableEditor = false;

    bb3d::Log::Init(config);
    BB_CORE_INFO("--- Unit Test 36: Renderer Hot-Path Optimization (Compact RenderCommand & Early Frustum Culling) ---");

    // 1. Validate RenderCommand memory footprint and sort ordering
    {
        BB_CORE_INFO("Test 1: Validating RenderCommand memory footprint and sort ordering...");
        assert(sizeof(bb3d::RenderCommand) == 24);

        std::vector<bb3d::RenderCommand> commands;
        commands.reserve(10);

        // Dummy pointers for comparison test
        auto* matA = reinterpret_cast<bb3d::Material*>(0x1000);
        auto* matB = reinterpret_cast<bb3d::Material*>(0x2000);
        auto* mesh1 = reinterpret_cast<bb3d::Mesh*>(0x0100);
        auto* mesh2 = reinterpret_cast<bb3d::Mesh*>(0x0200);

        commands.push_back({ .material = matB, .mesh = mesh2, .type = bb3d::MaterialType::Unlit, .transformIndex = 3, .castShadows = 1 });
        commands.push_back({ .material = matA, .mesh = mesh1, .type = bb3d::MaterialType::PBR, .transformIndex = 1, .castShadows = 0 });
        commands.push_back({ .material = matA, .mesh = mesh2, .type = bb3d::MaterialType::PBR, .transformIndex = 0, .castShadows = 1 });
        commands.push_back({ .material = matA, .mesh = mesh1, .type = bb3d::MaterialType::Highlight, .transformIndex = 2, .castShadows = 0 });

        std::ranges::sort(commands, [](const bb3d::RenderCommand& a, const bb3d::RenderCommand& b) noexcept {
            if (a.type != b.type) return a.type < b.type;
            if (a.material != b.material) return a.material < b.material;
            return a.mesh < b.mesh;
        });

        // Verify sorted order: PBR (0) < Unlit (1) < Highlight (5)
        assert(commands[0].type == bb3d::MaterialType::PBR);
        assert(commands[1].type == bb3d::MaterialType::PBR);
        assert(commands[2].type == bb3d::MaterialType::Unlit);
        assert(commands[3].type == bb3d::MaterialType::Highlight);

        // Verify secondary sorting: mesh1 < mesh2 for same material
        assert(commands[0].mesh == mesh1);
        assert(commands[0].transformIndex == 1);
        assert(commands[1].mesh == mesh2);
        assert(commands[1].transformIndex == 0);

        BB_CORE_INFO("Test 1: RenderCommand DOD sorting verified successfully (24-byte compact items).");
    }

    // 2. Integration Test with Engine, Renderer, Scene, and Early Frustum Culling
    auto engine = bb3d::Engine::Create(config);
    if (!engine) {
        BB_CORE_ERROR("Failed to create engine instance");
        return -1;
    }

    {
        auto scene = engine->CreateScene();
        engine->SetActiveScene(scene);
        auto& context = engine->GetVulkanContext();
        auto& renderer = engine->GetRenderer();

        // Camera looking along -Z from (0, 0, 5)
        auto cameraEntity = scene->createEntity("TestCamera");
        auto cam = bb3d::CreateRef<bb3d::Camera>(60.0f, 640.0f / 480.0f, 0.1f, 100.0f);
        cam->setPosition(glm::vec3(0.0f, 0.0f, 5.0f));
        cam->lookAt(glm::vec3(0.0f, 0.0f, 0.0f));
        cameraEntity.add<bb3d::CameraComponent>(cam);

        // Light for shadows
        auto lightEntity = scene->createEntity("DirectionalLight");
        lightEntity.add<bb3d::LightComponent>();
        auto& light = lightEntity.get<bb3d::LightComponent>();
        light.type = bb3d::LightType::Directional;
        light.castShadows = true;

        // Visible Entity 1: Cube in front of the camera
        auto visibleCube = bb3d::MeshGenerator::createCube(context, 1.0f);
        auto visibleEntity = scene->createEntity("VisibleCube");
        visibleEntity.at(glm::vec3(0.0f, 0.0f, 0.0f));
        visibleEntity.add<bb3d::MeshComponent>(std::move(visibleCube));

        // Culled Entity 2: Behind camera (z = 500.0f), does NOT cast shadows
        auto culledCube = bb3d::MeshGenerator::createCube(context, 1.0f);
        auto culledEntity = scene->createEntity("CulledCube");
        culledEntity.at(glm::vec3(0.0f, 0.0f, 500.0f));
        culledEntity.add<bb3d::MeshComponent>(std::move(culledCube));
        auto& culledMeshComp = culledEntity.get<bb3d::MeshComponent>();
        culledMeshComp.castShadows = false;

        // Shadow Caster Entity 3: Outside view frustum (far off to the side) but within shadowFarZ range
        auto shadowCasterCube = bb3d::MeshGenerator::createCube(context, 1.0f);
        auto shadowCasterEntity = scene->createEntity("ShadowCasterCube");
        shadowCasterEntity.at(glm::vec3(150.0f, 0.0f, 0.0f));
        shadowCasterEntity.add<bb3d::MeshComponent>(std::move(shadowCasterCube));
        auto& scMeshComp = shadowCasterEntity.get<bb3d::MeshComponent>();
        scMeshComp.castShadows = true;

        BB_CORE_INFO("Test 2: Rendering frame with hotpath optimizations...");
        bool frameRendered = renderer.render(*scene);
        assert(frameRendered && "Renderer::render must succeed!");

        renderer.submitAndPresent();
        BB_CORE_INFO("Test 2: Frame rendered and presented successfully without errors.");

        scene->getRegistry().clear();
        engine->SetActiveScene(nullptr);
    }

    engine->Shutdown();
    BB_CORE_INFO("Unit Test 36: All hotpath renderer optimizations PASSED cleanly!");
    return 0;
}
