#include "bb3d/core/Engine.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/scene/OrbitCamera.hpp"
#include "bb3d/render/MeshGenerator.hpp"
#include "bb3d/core/Log.hpp"

int main() {
    // 1. Initialisation du moteur avec config fluide (Nouveau Builder API)
    auto engine = bb3d::Engine::Create(bb3d::EngineConfig()
        .title("BB3D - Hello World 2.0 (OBJ + GLTF)")
        .resolution(1280, 720)
        .vsync(true)
    );

    // 2. Création de la scène
    auto scene = engine->CreateScene();
    engine->SetActiveScene(scene);

    // 3. Ajout d'une caméra (Composant)
    auto cameraEntity = scene->createEntity("MainCamera");
    auto orbitCam = bb3d::CreateRef<bb3d::OrbitCamera>(45.0f, 1280.0f/720.0f, 0.1f, 500.0f);
    orbitCam->setTarget({0, 2, 0});
    orbitCam->rotate(0.0f, 300.0f); 
    orbitCam->zoom(-10.0f); // Reculer un peu pour tout voir
    cameraEntity.add<bb3d::CameraComponent>(orbitCam);

    // Ajout d'un script de contrôle
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

    // 4. Chargement des Assets
    auto& assets = engine->assets();
    
    // Modèle OBJ (Test 10)
    auto houseModel = assets.load<bb3d::Model>("assets/models/house.obj");
    auto brickTexture = assets.load<bb3d::Texture>("assets/textures/Bricks092_1K-JPG_Color.jpg");
    
    // Assigner la texture manuellement aux meshes de la maison
    for (auto& mesh : houseModel->getMeshes()) {
        mesh->setTexture(brickTexture);
    }
    
    houseModel->normalize();
    
    // Modèle GLTF (Test 11)
    auto antModel = assets.load<bb3d::Model>("assets/models/ant.glb");
    antModel->normalize(glm::vec3(3.0f));

    // 5. Création des Entités
    
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

    BB_CORE_INFO("Scène Hello World complétée avec OBJ et GLTF !");

    // 6. Lancement
    engine->Run();

    // 7. Nettoyage explicite des références locales avant l'arrêt du moteur
    // Sinon ces ressources (Ref) tenteraient de se libérer après la destruction de l'allocateur Vulkan.
    scene->getRegistry().clear(); // Détruire tous les composants (incluant les Refs vers Mesh/Model)
    
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
