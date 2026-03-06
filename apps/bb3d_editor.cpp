#include "bb3d/core/Engine.hpp"
#include "bb3d/physics/PhysicsWorld.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/render/MeshGenerator.hpp"
#include "bb3d/core/IconsFontAwesome6.h"
#include <glm/gtc/matrix_transform.hpp>

using namespace bb3d;

/**
 * @brief Application Éditeur officielle du moteur biobazard3d.
 * Cette application présente une scène complexe avec physique, instancing, PBR
 * et l'interface d'édition intégrée (Viewport, Hierarchy, Inspector).
 */
int main() {
    auto engine = Engine::Create(EngineConfig()
        .title(ICON_FA_GEAR " biobazard3d - Editor " ICON_FA_ROCKET)
        .resolution(1600, 900)
        .vsync(true)
        .enableOffscreenRendering(true) // Requis pour le Viewport ImGui
        .renderScale(1.0f)
        .enablePhysics(PhysicsBackend::Jolt)
    );

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
    auto groundMat = CreateRef<UnlitMaterial>(engine->graphics());
    groundMesh->setMaterial(groundMat);
    
    auto ground = scene->createEntity("Ground Plane");
    ground.add<MeshComponent>(groundMesh, "", PrimitiveType::Plane);
    ground.add<RigidBodyComponent>().get<RigidBodyComponent>().type = BodyType::Static;
    ground.add<BoxColliderComponent>().get<BoxColliderComponent>().halfExtents = {100.0f, 0.1f, 100.0f};
    engine->physics().createRigidBody(ground);

    // --- 4. Modèles & Instancing ---
    std::vector<std::string> planePaths = {
        "assets/models/planes/Plane01/Plane01.obj",
        "assets/models/planes/Plane02/Plane02.obj"
    };

    auto rotScript = [](Entity ent, float dt) {
        auto& t = ent.get<TransformComponent>();
        t.rotation.y += dt * 0.2f;
    };

    auto firstModel = engine->assets().load<Model>(planePaths[0]);
    if (firstModel) {
        firstModel->normalize({5.0f, 5.0f, 5.0f});
        for (int i = 0; i < 20; ++i) {
            float x = (float)(i % 5) * 15.0f - 15.0f;
            float z = (float)(i / 5) * 15.0f + 10.0f;
            auto e = scene->createEntity("Instanced Plane");
            e.at({x, 10.0f, z}).add<ModelComponent>(firstModel, planePaths[0]);
            e.add<NativeScriptComponent>(rotScript);
        }
    }

    // Fourmi Géante (glTF)
    auto ant = scene->createModelEntity("Giant Ant (glTF)", "assets/models/ant.glb", {0, 2, -40}, {15.0f, 15.0f, 15.0f}); 
    if (ant) ant.add<NativeScriptComponent>(rotScript);

    // --- 5. Objets Physiques Dynamiques ---
    auto cubeMesh = Ref<Mesh>(MeshGenerator::createCube(engine->graphics(), 1.0f).release());
    auto physMat = CreateRef<PBRMaterial>(engine->graphics());
    physMat->setColor({0.8f, 0.1f, 0.1f});
    cubeMesh->setMaterial(physMat);

    for (int i = 0; i < 5; ++i) {
        auto ent = scene->createEntity("Falling Cube");
        ent.at({(float)i * 2.5f - 4.0f, 20.0f, 0.0f});
        ent.add<MeshComponent>(cubeMesh, "", PrimitiveType::Cube);
        ent.add<BoxColliderComponent>().get<BoxColliderComponent>().halfExtents = {0.5f, 0.5f, 0.5f};
        auto& rb = ent.add<RigidBodyComponent>().get<RigidBodyComponent>();
        rb.type = BodyType::Dynamic;
        rb.mass = 2.0f;
        engine->physics().createRigidBody(ent);
    }

    // --- 6. Logique de Contrôle (Switch Caméra) ---
    scene->createEntity("Editor Logic").add<NativeScriptComponent>([orbitCamEnt, fpsCamEnt, &engine](Entity /*ent*/, float /*dt*/) mutable {
        if (engine->input().isKeyJustPressed(Key::C)) {
            auto& orbitComp = orbitCamEnt.get<CameraComponent>();
            auto& fpsComp = fpsCamEnt.get<CameraComponent>();
            orbitComp.active = !orbitComp.active;
            fpsComp.active = !orbitComp.active;
            BB_CORE_INFO("Editor: Camera switched to {}", orbitComp.active ? "Orbit" : "FPS");
        }
    });

    BB_CORE_INFO("bb3d Editor ready.");
    engine->Run();

    // Cleanup explicite
    engine->SetActiveScene(nullptr);
    scene.reset();
    engine->Shutdown();

    return 0;
}
