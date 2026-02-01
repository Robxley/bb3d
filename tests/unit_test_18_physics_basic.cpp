#include "bb3d/core/Engine.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/scene/OrbitCamera.hpp"
#include "bb3d/render/MeshGenerator.hpp"
#include "bb3d/render/Material.hpp"
#include "bb3d/core/Log.hpp"
#include "bb3d/physics/PhysicsWorld.hpp"

using namespace bb3d;

int main() {
    auto engine = Engine::Create(EngineConfig()
        .title("BB3D - Basic Physics Test (Jolt)")
        .resolution(1280, 720)
        .vsync(true)
        .enablePhysics(PhysicsBackend::Jolt)
    );

    auto scene = engine->CreateScene();
    engine->SetActiveScene(scene);

    // 1. Caméra
    auto cameraEntity = scene->createOrbitCamera("MainCamera", 45.0f, 1280.0f/720.0f, {0, 5, 0}, 20.0f);

    // 2. Lumière
    scene->createDirectionalLight("Sun", {1, 1, 1}, 2.0f, {-45, 45, 0});

    // 3. Sol (Statique)
    auto ground = scene->createEntity("Ground");
    ground.at({0, 0, 0});
    auto groundMesh = Ref<Mesh>(MeshGenerator::createCheckerboardPlane(engine->graphics(), 50.0f, 10).release());
    ground.add<MeshComponent>(groundMesh);
    
    // Physique du sol
    ground.add<RigidBodyComponent>();
    auto& groundRB = ground.get<RigidBodyComponent>();
    groundRB.type = BodyType::Static;
    ground.add<BoxColliderComponent>(glm::vec3(25.0f, 0.1f, 25.0f));
    engine->physics().createRigidBody(ground);

    // 4. Boîtes (Dynamiques)
    auto cubeMesh = Ref<Mesh>(MeshGenerator::createCube(engine->graphics(), 1.0f).release());
    auto redMat = CreateRef<PBRMaterial>(engine->graphics());
    redMat->setAlbedoMap(nullptr); // Use default white
    PBRParameters params; params.baseColorFactor = {1, 0, 0, 1};
    redMat->setParameters(params);
    cubeMesh->setMaterial(redMat);

    for (int i = 0; i < 5; ++i) {
        auto cube = scene->createEntity("FallingCube");
        cube.at({0.0f, 10.0f + i * 2.0f, 0.0f});
        cube.add<MeshComponent>(cubeMesh);
        
        cube.add<RigidBodyComponent>();
        auto& rb = cube.get<RigidBodyComponent>();
        rb.type = BodyType::Dynamic;
        rb.mass = 1.0f;
        cube.add<BoxColliderComponent>(glm::vec3(0.5f, 0.5f, 0.5f));
        
        engine->physics().createRigidBody(cube);
    }

    BB_CORE_INFO("Physics Test Ready: Cubes should fall on the ground.");

    engine->Run();

    return 0;
}