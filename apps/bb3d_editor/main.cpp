#include "bb3d/core/Engine.hpp"
#include "bb3d/physics/PhysicsWorld.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/render/MeshGenerator.hpp"
#include "bb3d/core/IconsFontAwesome6.h"
#include <glm/gtc/matrix_transform.hpp>

#if defined(BB3D_ENABLE_EDITOR)
using namespace bb3d;

/**
 * @brief Application Éditeur officielle du moteur biobazard3d.
 * Cette application présente une scène complexe avec physique, instancing, PBR
 * et l'interface d'édition intégrée (Viewport, Hierarchy, Inspector).
 */
int main() {
    auto config = Config::Load("config/engine_config.json");
    config.title(ICON_FA_GEAR " biobazard3d - Editor " ICON_FA_ROCKET)
          .resolution(1600, 900)
          .vsync(true)
          .enableOffscreenRendering(true) // Required for the ImGui Viewport
          .renderScale(1.0f)
          .enablePhysics(PhysicsBackend::Jolt)
          .enableEditor(true); // Editor app: Must have editor enabled
          
    config.graphics.setShadows(true, 4096, 4, true);

    auto engine = Engine::Create(config);

    { // Scope to ensure all Refs (scene, mesh, models) are destroyed BEFORE engine->Shutdown()
        auto scene = engine->CreateScene();
        engine->SetActiveScene(scene);


    // --- 1. Caméras ---
    // Caméra Orbitale (Active par défaut dans l'éditeur)
    auto orbitCamEnt = scene->createOrbitCamera("Orbit Camera", 45.0f, 1600.0f/900.0f, {0, 2, 0}, 35.0f);
    orbitCamEnt->rotationSpeed = {0.2f, 0.2f};
    orbitCamEnt->zoomSpeed = 2.0f;

    // Caméra FPS (Secondaire)
    auto fpsCamEnt = scene->createFPSCamera("FPS Camera", 60.0f, 1600.0f/900.0f, {0, 5, 20}, engine.get());
    fpsCamEnt->movementSpeed = {8.0f, 5.0f, 15.0f};
    fpsCamEnt.get<CameraComponent>().active = false;

    // --- 2. Environnement ---
    scene->createSkySphere("Sky Environment", "assets/textures/skybox_sphere_wood_diffuse.jpeg");
    scene->createDirectionalLight("Sun", {1, 1, 0.95f}, 3.0f, {-45.0f, 45.0f, 0.0f});

    // --- 3. Sol & Physique Statique ---
    auto groundMesh = Ref<Mesh>(MeshGenerator::createCheckerboardPlane(engine->graphics(), 200.0f, 40).release());
    auto groundMat = CreateRef<PBRMaterial>(engine->graphics());
    groundMat->setColor({0.5f, 0.5f, 0.5f});
    groundMesh->setMaterial(groundMat);
    
    auto ground = scene->createEntity("Ground Plane");
    ground.add<MeshComponent>(groundMesh, "", PrimitiveType::Plane);
    auto& groundPhys = ground.add<PhysicsComponent>().get<PhysicsComponent>();
    groundPhys.type = BodyType::Static;
    groundPhys.colliderType = ColliderType::Box;
    groundPhys.boxHalfExtents = {100.0f, 0.1f, 100.0f};
    engine->physics().createRigidBody(ground);

    // --- 4. Modèles & Instancing ---
    std::vector<std::string> planePaths = {
        "assets/models/planes/Plane01/Plane01.obj",
        "assets/models/planes/Plane02/Plane02.obj"
    };

    // SimpleAnimationComponent will be used instead of rotation scripts for the demo
    ModelLoadConfig planeConfig;
    planeConfig.recalculateNormals = true;

    auto firstModel = engine->assets().load<Model>(planePaths[0], planeConfig);
    if (firstModel) {
        firstModel->normalize({5.0f, 5.0f, 5.0f});
        for (int i = 0; i < 20; ++i) {
            float x = (float)(i % 5) * 15.0f - 15.0f;
            float z = (float)(i / 5) * 15.0f + 10.0f;
            auto e = scene->createEntity("Instanced Plane");
            e.at({x, 10.0f, z}).add<ModelComponent>(firstModel, planePaths[0]);
            e.add<SimpleAnimationComponent>().get<SimpleAnimationComponent>().speed = 0.2f;
            auto& phys = e.add<PhysicsComponent>().get<PhysicsComponent>();
            phys.type = BodyType::Kinematic;
            phys.colliderType = ColliderType::Box;
            phys.boxHalfExtents = { 3.0f, 1.0f, 3.0f };
            engine->physics().createRigidBody(e);
        }
    }

    // Fourmi Géante (glTF)
    auto ant = scene->createModelEntity("Giant Ant (glTF)", "assets/models/ant.glb", {0, 2, -40}, {15.0f, 15.0f, 15.0f}); 
    if (ant) ant.add<SimpleAnimationComponent>().get<SimpleAnimationComponent>().speed = 0.2f;

    // --- 5. Objets Physiques Dynamiques ---
    auto cubeMesh = Ref<Mesh>(MeshGenerator::createCube(engine->graphics(), 1.0f).release());
    auto physMat = CreateRef<PBRMaterial>(engine->graphics());
    physMat->setColor({0.8f, 0.1f, 0.1f});
    cubeMesh->setMaterial(physMat);

    for (int i = 0; i < 5; ++i) {
        auto ent = scene->createEntity("Falling Cube");
        ent.at({(float)i * 2.5f - 4.0f, 20.0f, 0.0f});
        ent.add<MeshComponent>(cubeMesh, "", PrimitiveType::Cube);
        auto& phys = ent.add<PhysicsComponent>().get<PhysicsComponent>();
        phys.colliderType = ColliderType::Box;
        phys.boxHalfExtents = {0.5f, 0.5f, 0.5f};
        phys.type = BodyType::Dynamic;
        phys.mass = 2.0f;
        engine->physics().createRigidBody(ent);
    }

    // --- 6. Rocket Debug Lineup ---
    std::vector<std::string> rocketPaths = {
        "assets/models/rockets/rocket_1/rocket.obj",
        "assets/models/rockets/rocket_2/scene.gltf",
        "assets/models/rockets/rocket_3/scene.gltf",
        "assets/models/rockets/rocket_4/scene.gltf",
        "assets/models/rockets/rocket_5/scene.gltf",
        "assets/models/rockets/rocket_6/scene.gltf"
    };

    for (int i = 0; i < rocketPaths.size(); ++i) {
        try {
            auto model = engine->assets().load<Model>(rocketPaths[i]);
            model->normalize(glm::vec3(2.0f)); 
            
            auto eVisual = scene->createEntity("RocketDebug_" + std::to_string(i+1));
            eVisual.at({-15.0f + i * 6.0f, 15.0f, -60.0f}); // Positioned for editor view
            eVisual.add<ModelComponent>(model, rocketPaths[i]);
            BB_CORE_INFO("Editor: Rocket {0} added to debug lineup", i+1);
        } catch(...) {
            BB_CORE_WARN("Editor: Failed to load rocket {0} for debug", i+1);
        }
    }

    // --- 7. Control Logic (Camera Switch) ---
    scene->createEntity("Editor Logic").add<NativeScriptComponent>([orbitCamEnt, fpsCamEnt, enginePtr = engine.get()](Entity /*ent*/, float /*dt*/) mutable {
        if (enginePtr->input().isKeyJustPressed(Key::C)) {
            auto& orbitComp = orbitCamEnt.get<CameraComponent>();
            auto& fpsComp = fpsCamEnt.get<CameraComponent>();
            orbitComp.active = !orbitComp.active;
            fpsComp.active = !orbitComp.active;
            BB_CORE_INFO("Editor: Camera switched to {}", orbitComp.active ? "Orbit" : "FPS");
        }
    });

    engine->Run();

    // Ensure GPU is idle before resource-owning Refs go out of scope
    if (engine->graphics().getDevice()) {
        engine->graphics().getDevice().waitIdle();
    }

    // Resetting references explicitly within the scope
    engine->SetActiveScene(nullptr);
    } // End of scene scope


    return 0;
}

#else
#include <iostream>
int main() {
    std::cerr << "Error: bb3d_editor requires BB3D_ENABLE_EDITOR to be enabled at compile time." << std::endl;
    return 1;
}
#endif

