#include "bb3d/core/Engine.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/scene/OrbitCamera.hpp"
#include "bb3d/render/MeshGenerator.hpp"
#include "bb3d/core/Log.hpp"
#include "bb3d/physics/PhysicsWorld.hpp"
#include "bb3d/audio/AudioSystem.hpp"

int main() {
    // 1. Initialisation du moteur avec toutes les fonctionnalités
    auto engine = bb3d::Engine::Create(bb3d::EngineConfig()
        .title("BB3D - Full Engine Test")
        .resolution(1280, 720)
        .vsync(true)
        .enablePhysics(true)
        .enableAudio(false)
        .enableEditor(false)
    );

    // 2. Création de la scène
    auto scene = engine->CreateScene();
    engine->SetActiveScene(scene);

    // 3. Ajout d'une caméra orbitale
    auto cameraEntity = scene->createEntity("MainCamera");
    auto orbitCam = bb3d::CreateRef<bb3d::OrbitCamera>(45.0f, 1280.0f/720.0f, 0.1f, 500.0f);
    orbitCam->setTarget({0, 2, 0});
    orbitCam->rotate(0.0f, 300.0f);
    orbitCam->zoom(-10.0f);
    cameraEntity.add<bb3d::CameraComponent>(orbitCam);

    // 4. Ajout d'un script de contrôle pour la caméra
    cameraEntity.add<bb3d::NativeScriptComponent>([eng = engine.get()](bb3d::Entity entity, float dt) {
        auto& camComp = entity.get<bb3d::CameraComponent>();
        auto* orbit = dynamic_cast<bb3d::OrbitCamera*>(camComp.camera.get());
        if (!orbit) return;

        auto& input = eng->input();
        if (input.isMouseButtonPressed(bb3d::Mouse::Left)) {
            glm::vec2 delta = input.getMouseDelta();
            orbit->rotate(delta.x * 5.0f, -delta.y * 5.0f);
        }
        orbit->zoom(input.getMouseScroll().y);
    });

    // 5. Chargement des Assets
    auto& assets = engine->assets();
    
    // Modèle OBJ
    auto houseModel = assets.load<bb3d::Model>("assets/models/house.obj");
    auto brickTexture = assets.load<bb3d::Texture>("assets/textures/Bricks092_1K-JPG_Color.jpg");
    
    // Assigner la texture manuellement aux meshes de la maison
    for (auto& mesh : houseModel->getMeshes()) {
        mesh->setTexture(brickTexture);
    }
    
    houseModel->normalize();

    // Modèle GLTF
    auto antModel = assets.load<bb3d::Model>("assets/models/ant.glb");
    antModel->normalize(glm::vec3(3.0f));

    // 6. Création des Entités
    
    // Le Cube rouge par défaut
    auto cubeMesh = bb3d::Ref<bb3d::Mesh>(bb3d::MeshGenerator::createCube(engine->graphics(), 1.0f, {1, 0, 0}).release());
    scene->createEntity("RedCube")
        .at({0, 0.5f, 0})
        .add<bb3d::MeshComponent>(cubeMesh, "", bb3d::PrimitiveType::Cube);

    // La Maison (OBJ) - Normalisée (taille ~1)
    scene->createEntity("House")
        .at({-2.0f, 0.5f, 0.0f})
        .add<bb3d::ModelComponent>(houseModel);

    // La Fourmi (GLTF) - Normalisée (taille ~1)
    scene->createEntity("Ant")
        .at({2.0f, 0.5f, 0.0f})
        .add<bb3d::ModelComponent>(antModel);

    // Le Sol
    auto floorMesh = bb3d::Ref<bb3d::Mesh>(bb3d::MeshGenerator::createCheckerboardPlane(engine->graphics(), 50.0f, 50).release());
    scene->createEntity("Floor")
        .add<bb3d::MeshComponent>(floorMesh, "", bb3d::PrimitiveType::Plane);

    // 7. Ajout de la physique
    auto physicsWorld = engine->GetPhysicsWorld();
    if (physicsWorld) {
        // Ajout d'une sphère physique
        auto sphereEntity = scene->createEntity("PhysicsSphere");
        sphereEntity.at({0, 5.0f, 0});
        sphereEntity.add<bb3d::PhysicsComponent>(bb3d::ColliderType::Sphere, 1.0f);
        sphereEntity.add<bb3d::MeshComponent>(bb3d::MeshGenerator::createSphere(engine->graphics(), 1.0f, {0, 1, 0}), "", bb3d::PrimitiveType::Sphere);
        
        // Ajout d'un plan physique pour le sol
        auto physicsFloorEntity = scene->createEntity("PhysicsFloor");
        physicsFloorEntity.add<bb3d::PhysicsComponent>(bb3d::ColliderType::Box, {50.0f, 0.5f, 50.0f});
        physicsFloorEntity.add<bb3d::MeshComponent>(bb3d::MeshGenerator::createCube(engine->graphics(), {50.0f, 0.5f, 50.0f}, {0.5f, 0.5f, 0.5f}), "", bb3d::PrimitiveType::Cube);
    }

    // 8. Ajout de l'audio
    auto audioSystem = engine->GetAudioSystem();
    if (audioSystem) {
        audioSystem->playSound("assets/sounds/test.wav", {0, 0, 0}, 1.0f);
    }

    BB_CORE_INFO("Scène Full Engine Test complétée avec toutes les fonctionnalités !");

    // 9. Lancement
    engine->Run();

    // 10. Nettoyage explicite des références locales avant l'arrêt du moteur
    scene->getRegistry().clear();
    
    houseModel.reset();
    brickTexture.reset();
    antModel.reset();
    cubeMesh.reset();
    floorMesh.reset();
    
    engine->SetActiveScene(nullptr);
    scene.reset();

    engine->Shutdown();
    bb3d::Material::Cleanup();

    return 0;
}