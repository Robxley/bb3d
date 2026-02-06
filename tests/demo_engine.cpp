#include "bb3d/core/Engine.hpp"
#include "bb3d/physics/PhysicsWorld.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/render/MeshGenerator.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

using namespace bb3d;

int main() {
    auto engine = Engine::Create(EngineConfig()
        .title("BB3D Demo - Offscreen 50%")
        .resolution(1280, 720)
        .vsync(true)
        .enableOffscreenRendering(true)
        .renderScale(0.5f)
        .enablePhysics(PhysicsBackend::Jolt)
    );

    auto scene = engine->CreateScene();
    engine->SetActiveScene(scene);

    // Caméra Orbitale (Active par défaut)
    auto orbitCamEnt = scene->createOrbitCamera("OrbitCamera", 45.0f, 1280.0f/720.0f, {0, 2, 0}, 35.0f);
    orbitCamEnt->rotationSpeed = {0.2f, 0.2f};
    orbitCamEnt->zoomSpeed = 2.0f;

    // Caméra FPS
    auto fpsCamEnt = scene->createFPSCamera("FPSCamera", 60.0f, 1280.0f/720.0f, {0, 5, 20});
    fpsCamEnt->movementSpeed = {8.0f, 5.0f, 15.0f};
    fpsCamEnt->rotationSpeed = {0.15f, 0.15f};
    fpsCamEnt.get<CameraComponent>().active = false;

    // 2. Environnement
    scene->createSkySphere("SkyEnvironment", "assets/textures/skybox_sphere_wood_diffuse.jpeg");

    // 2. Sol avec Physique
    auto groundMesh = Ref<Mesh>(MeshGenerator::createCheckerboardPlane(engine->graphics(), 200.0f, 40).release());
    auto groundMat = CreateRef<UnlitMaterial>(engine->graphics());
    groundMesh->setMaterial(groundMat);
    
    auto ground = scene->createEntity("Ground");
    ground.add<MeshComponent>(groundMesh);
    ground.add<RigidBodyComponent>().get<RigidBodyComponent>().type = BodyType::Static;
    ground.add<BoxColliderComponent>().get<BoxColliderComponent>().halfExtents = {100.0f, 0.1f, 100.0f};
    engine->physics().createRigidBody(ground);

    // 3. Modèles d'Avions
    std::vector<std::string> planePaths = {
        "assets/models/planes/Plane01/Plane01.obj",
        "assets/models/planes/Plane02/Plane02.obj",
        "assets/models/planes/Plane03/Plane03.obj",
        "assets/models/planes/Plane05/Plane05.obj",
        "assets/models/planes/Plane06/Plane06.obj"
    };

    auto rotScript = [](Entity ent, float dt) {
        auto& t = ent.get<TransformComponent>();
        t.rotation.y += dt * 0.5f;
    };

    auto firstModel = engine->assets().load<Model>(planePaths[0]);
    if (firstModel) {
        firstModel->normalize({5.0f, 5.0f, 5.0f});
        firstModel->releaseCPUData(); 
        for (int i = 0; i < 50; ++i) {
            float x = (float)(i % 10) * 8.0f - 20.0f;
            float z = (float)(i / 10) * 8.0f + 10.0f;
            auto e = scene->createEntity("InstancedPlane");
            e.at({x, 15.0f, z}).add<ModelComponent>(firstModel);
            e.add<NativeScriptComponent>(rotScript);
        }
    }

    for (size_t i = 1; i < planePaths.size(); ++i) {
        auto e = scene->createModelEntity("UniquePlane", planePaths[i], {(float)(i * 10 - 20), 20.0f, -10.0f}, {8.0f, 8.0f, 8.0f});
        if (e) e.add<NativeScriptComponent>(rotScript);
    }

    // --- 4. Objets Physiques Interactifs ---
    auto cubeMesh = Ref<Mesh>(MeshGenerator::createCube(engine->graphics(), 1.0f).release());
    auto sphereMesh = Ref<Mesh>(MeshGenerator::createSphere(engine->graphics(), 0.5f, 16).release());
    
    auto physMat = CreateRef<PBRMaterial>(engine->graphics());
    PBRParameters p; p.baseColorFactor = {0.8f, 0.2f, 0.2f, 1.0f};
    physMat->setParameters(p);
    cubeMesh->setMaterial(physMat);
    sphereMesh->setMaterial(physMat);

    // 3 Cubes qui tombent automatiquement au dÃ©but
    for (int i = 0; i < 3; ++i) {
        auto ent = scene->createEntity("AutoFallingCube");
        ent.at({(float)i * 2.0f - 2.0f, 25.0f, 5.0f});
        ent.add<MeshComponent>(cubeMesh);
        ent.add<BoxColliderComponent>().get<BoxColliderComponent>().halfExtents = {0.5f, 0.5f, 0.5f};
        auto& rb = ent.add<RigidBodyComponent>().get<RigidBodyComponent>();
        rb.type = BodyType::Dynamic;
        rb.mass = 2.0f;
        engine->physics().createRigidBody(ent);
    }

    // Script Spawner de Physique
    scene->createEntity("PhysSpawner").add<NativeScriptComponent>([cubeMesh, sphereMesh, &eng = *engine, scene](Entity /*e*/, float /*dt*/) {
        static bool pPressedLastFrame = false;
        bool pIsPressed = eng.input().isKeyPressed(Key::P);
        
        if (pIsPressed && !pPressedLastFrame) {
            static int count = 0;
            auto spawnPos = glm::vec3(0, 30, 0) + glm::vec3((count % 3) - 1, 0, (count / 3 % 3) - 1) * 2.0f;
            auto ent = scene->createEntity("SpawnedPhys");
            ent.at(spawnPos);
            
            if (count % 2 == 0) {
                ent.add<MeshComponent>(cubeMesh);
                ent.add<BoxColliderComponent>().get<BoxColliderComponent>().halfExtents = {0.5f, 0.5f, 0.5f};
            } else {
                ent.add<MeshComponent>(sphereMesh);
                ent.add<SphereColliderComponent>().get<SphereColliderComponent>().radius = 0.5f;
            }
            
            auto& rb = ent.add<RigidBodyComponent>().get<RigidBodyComponent>();
            rb.type = BodyType::Dynamic;
            rb.mass = 2.0f;
            eng.physics().createRigidBody(ent);
            count++;
        }
        pPressedLastFrame = pIsPressed;
    });

    // Script de switch caméra et contrôles
    scene->createEntity("SystemManager").add<NativeScriptComponent>([orbitCamEnt, fpsCamEnt, eng = engine.get()](Entity /*ent*/, float /*dt*/) mutable {
        static bool cPressedLastFrame = false;
        auto& input = eng->input();
        
        if (input.isKeyJustPressed(Key::C)) {
            auto& orbitComp = orbitCamEnt.get<CameraComponent>();
            auto& fpsComp = fpsCamEnt.get<CameraComponent>();
            orbitComp.active = !orbitComp.active;
            fpsComp.active = !orbitComp.active;
            BB_CORE_INFO("Camera Switched!");
        }
    });

    // --- 5. Fourmi Géante (glTF) ---
    auto ant = scene->createModelEntity("GiantAnt", "assets/models/ant.glb", {0, 2, -80}, {20.0f, 20.0f, 20.0f}); 
    if (ant) {
        ant.add<NativeScriptComponent>(rotScript);
    }

    // --- 6. Lumières ---
    scene->createDirectionalLight("Sun", {1, 1, 0.9f}, 3.0f, {-45.0f, 45.0f, 0.0f});
    scene->createPointLight("RedLight", {1.0f, 0.2f, 0.2f}, 150.0f, 30.0f, {-15.0f, 8.0f, 0.0f});
    scene->createPointLight("GreenLight", {0.2f, 1.0f, 0.2f}, 150.0f, 30.0f, {0.0f, 8.0f, 15.0f});
    scene->createPointLight("BlueLight", {0.2f, 0.2f, 1.0f}, 150.0f, 30.0f, {15.0f, 8.0f, 0.0f});

    BB_CORE_INFO("Demo Engine Ready! (Press 'P' to spawn physics, 'C' to switch camera)");
    engine->Run();

    // Cleanup
    firstModel.reset();
    cubeMesh.reset();
    sphereMesh.reset();
    physMat.reset();
    groundMesh.reset();
    groundMat.reset();

    scene->getRegistry().clear();
    engine->SetActiveScene(nullptr);
    scene.reset();
    engine->Shutdown();
    Material::Cleanup();

    return 0;
}
