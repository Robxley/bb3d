#include "bb3d/core/Engine.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/scene/OrbitCamera.hpp"
#include "bb3d/render/MeshGenerator.hpp"
#include "bb3d/core/Log.hpp"

int main() {
    // 1. Initialisation du moteur avec config fluide
    bb3d::EngineConfig config;
    config.window.setTitle("BB3D - Hello World Simplified")
                 .setResolution(1280, 720);
    
    auto engine = bb3d::Engine::Create(config);

    // 2. Création de la scène
    auto scene = engine->CreateScene();
    engine->SetActiveScene(scene);

    // 3. Ajout d'une caméra (Composant)
    auto cameraEntity = scene->createEntity("MainCamera");
    auto orbitCam = bb3d::CreateRef<bb3d::OrbitCamera>(45.0f, 1280.0f/720.0f, 0.1f, 100.0f);
    orbitCam->setTarget({0, 0, 0});
    cameraEntity.add<bb3d::CameraComponent>(orbitCam);

    // 4. Ajout d'objets (API Fluide)
    // Note: createCube retourne un Scope, on le convertit en Ref pour le MeshComponent
    auto cubeMesh = bb3d::Ref<bb3d::Mesh>(bb3d::MeshGenerator::createCube(engine->graphics(), 1.0f, {1, 0, 0}).release());
    
    scene->createEntity("RedCube")
        .at({0, 0.5f, 0})
        .add<bb3d::MeshComponent>(cubeMesh);

    auto floorMesh = bb3d::Ref<bb3d::Mesh>(bb3d::MeshGenerator::createCheckerboardPlane(engine->graphics(), 20.0f, 20).release());
    scene->createEntity("Floor")
        .add<bb3d::MeshComponent>(floorMesh);

    BB_CORE_INFO("Hello World prêt ! Lancement du moteur...");

    // 5. Lancement automatique (Update + Render internes)
    engine->Run();

    return 0;
}