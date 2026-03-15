#include "bb3d/core/Engine.hpp"
#include "bb3d/physics/PhysicsWorld.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/render/MeshGenerator.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <string>

using namespace bb3d;

/**
 * @brief Application de vitrine (Showcase) du moteur biobazard3d.
 * Cette scène regroupe un exemple pour chaque fonctionnalité majeure :
 * - Rendu PBR & Skybox
 * - Physique Jolt (Gravité, Collisions)
 * - Chargement glTF & OBJ avec positionnement précis via AABB
 * - Instancing de modèles variés
 * - Système de Scripts Natifs
 * - Lumières Dynamiques
 * - Switch de caméras fonctionnel (Orbit/FPS)
 */
int main() {
    auto engine = Engine::Create(Config::Load("config/engine_config.json")
        .title("biobazard3d - Feature Showcase")
        .resolution(1280, 720)
        .vsync(true)
        .enableOffscreenRendering(false)
        .enablePhysics(PhysicsBackend::Jolt)
        .enableEditor(false) // Showcase app: Always without editor for maximum performance and clean UI
    );

    { // Scope to ensure all Refs are destroyed before engine goes out of scope
        auto scene = engine->CreateScene();
        engine->SetActiveScene(scene);


    // --- 1. Caméras (Deux caméras créées, seule l'orbitale est active par défaut) ---
    auto orbitCam = scene->createOrbitCamera("Orbit Camera", 45.0f, 1280.0f/720.0f, {0, 2, 0}, 45.0f);
    
    auto fpsCam = scene->createFPSCamera("FPS Camera", 60.0f, 1280.0f/720.0f, {0, 5, 20});
    fpsCam.get<CameraComponent>().active = false; // Inactive au départ

    // --- 2. Environnement & Lumières ---
    scene->createSkySphere("Sky", "assets/textures/skybox_sphere_wood_diffuse.jpeg");
    scene->createDirectionalLight("Sun", {1.0f, 0.95f, 0.8f}, 3.0f, {-45.0f, 45.0f, 0.0f});
    scene->createPointLight("RedLight", {1.0f, 0.2f, 0.2f}, 150.0f, 30.0f, {-15.0f, 8.0f, 0.0f});
    scene->createPointLight("BlueLight", {0.2f, 0.2f, 1.0f}, 150.0f, 30.0f, {15.0f, 8.0f, 0.0f});

    // --- 3. Sol Physique (Static) ---
    auto groundMesh = Ref<Mesh>(MeshGenerator::createCheckerboardPlane(engine->graphics(), 200.0f, 40).release());
    auto groundMat = CreateRef<UnlitMaterial>(engine->graphics());
    groundMesh->setMaterial(groundMat);
    
    auto ground = scene->createEntity("Ground");
    ground.add<MeshComponent>(groundMesh);
    auto& groundPhys = ground.add<PhysicsComponent>().get<PhysicsComponent>();
    groundPhys.type = BodyType::Static;
    groundPhys.colliderType = ColliderType::Box;
    groundPhys.boxHalfExtents = {100.0f, 0.1f, 100.0f};
    engine->physics().createRigidBody(ground);

    // --- 4. Showcase : Multi-Model Instancing (OBJ) ---
    std::vector<std::string> planePaths = {
        "assets/models/planes/Plane01/Plane01.obj",
        "assets/models/planes/Plane02/Plane02.obj",
        "assets/models/planes/Plane03/Plane03.obj",
        "assets/models/planes/Plane05/Plane05.obj",
        "assets/models/planes/Plane06/Plane06.obj"
    };

    std::vector<Ref<Model>> planeModels;
    for (const auto& path : planePaths) {
        auto model = engine->assets().load<Model>(path);
        if (model) {
            model->normalize({6.0f, 6.0f, 6.0f});
            planeModels.push_back(model);
        }
    }

    if (!planeModels.empty()) {
        for (int i = 0; i < 50; ++i) {
            auto model = planeModels[i % planeModels.size()];
            float x = (float)(i % 10) * 12.0f - 45.0f;
            float z = (float)(i / 10) * 12.0f + 15.0f;
            auto e = scene->createEntity("Instanced Plane");
            e.at({x, 15.0f, z}).add<ModelComponent>(model);
            e.add<SimpleAnimationComponent>().get<SimpleAnimationComponent>().speed = 0.3f;
            auto& phys = e.add<PhysicsComponent>().get<PhysicsComponent>();
            phys.type = BodyType::Kinematic;
            phys.colliderType = ColliderType::Box;
            phys.boxHalfExtents = { 3.0f, 1.0f, 3.0f };
            engine->physics().createRigidBody(e);
        }
    }

    // --- 5. Showcase : glTF avec positionnement précis via AABB ---
    auto antModel = engine->assets().load<Model>("assets/models/ant.glb");
    if (antModel) {
        antModel->normalize(glm::vec3(20.0f));
        const auto& bounds = antModel->getBounds();
        float yOffset = -bounds.min.y; 

        auto ant = scene->createEntity("Giant Ant");
        ant.at({0.0f, yOffset, -15.0f}).add<ModelComponent>(antModel);
        ant.add<SimpleAnimationComponent>().get<SimpleAnimationComponent>().speed = 0.1f;
    }

    // --- 6. Showcase : Physique Interactive (Dynamic) ---
    auto cubeMesh = Ref<Mesh>(MeshGenerator::createCube(engine->graphics(), 1.0f).release());
    auto physMat = CreateRef<PBRMaterial>(engine->graphics());
    physMat->setColor({0.8f, 0.2f, 0.2f});
    cubeMesh->setMaterial(physMat);

    for (int i = 0; i < 8; ++i) {
        auto ent = scene->createEntity("AutoFallingCube");
        ent.at({(float)i * 3.0f - 10.0f, 30.0f, 5.0f});
        ent.add<MeshComponent>(cubeMesh);
        auto& phys = ent.add<PhysicsComponent>().get<PhysicsComponent>();
        phys.colliderType = ColliderType::Box;
        phys.boxHalfExtents = {0.5f, 0.5f, 0.5f};
        phys.type = BodyType::Dynamic;
        phys.mass = 2.0f;
        engine->physics().createRigidBody(ent);
    }

    // Global script for inputs
    scene->createEntity("InputManager").add<NativeScriptComponent>([enginePtr = engine.get(), scene, cubeMesh](Entity /*e*/, float /*dt*/) {
        // P : Spawn cube
        if (enginePtr->input().isKeyJustPressed(Key::P)) {
            auto spawnPos = glm::vec3(0, 40, 0);
            auto ent = scene->createEntity("SpawnedPhys");
            ent.at(spawnPos).add<MeshComponent>(cubeMesh);
            auto& phys = ent.add<PhysicsComponent>().get<PhysicsComponent>();
            phys.colliderType = ColliderType::Box;
            phys.boxHalfExtents = {0.5f, 0.5f, 0.5f};
            phys.type = BodyType::Dynamic;
            phys.mass = 2.0f;
            enginePtr->physics().createRigidBody(ent);
        }

        // C : Switch between Orbit and FPS
        if (enginePtr->input().isKeyJustPressed(Key::C)) {
            auto view = scene->getRegistry().view<CameraComponent>();
            for (auto entity : view) {
                auto& cam = view.get<CameraComponent>(entity);
                cam.active = !cam.active;
            }
            BB_CORE_INFO("Input: Camera Switched!");
        }
    });


    BB_CORE_INFO("==================================================");
    BB_CORE_INFO(" BB3D SHOWCASE - READY (PURE 3D)");
    BB_CORE_INFO(" - Multiple Plane Models (OBJ Instancing)");
    BB_CORE_INFO(" - Giant Ant (GLTF properly grounded via AABB)");
    BB_CORE_INFO(" - Jolt Physics (Auto-falling and 'P' to spawn)");
    BB_CORE_INFO(" - Key 'C' : Switch Camera (Orbit / FPS)");
    BB_CORE_INFO("==================================================");

    engine->Run();

    // Ensure GPU is idle before resource-owning Refs go out of scope
    if (engine->graphics().getDevice()) {
        engine->graphics().getDevice().waitIdle();
    }

    engine->SetActiveScene(nullptr);
    } // End of scene scope

    return 0;
}
