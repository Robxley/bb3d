#include "bb3d/core/Engine.hpp"
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
    );

    auto scene = engine->CreateScene();
    engine->SetActiveScene(scene);

    // 1. Caméras
    auto orbitCamEnt = scene->createOrbitCamera("OrbitCamera", 45.0f, 1600.0f/900.0f, {0, 2, 0}, 30.0f);
    orbitCamEnt->rotationSpeed = {0.2f, 0.2f};
    orbitCamEnt->zoomSpeed = 2.0f;

    auto fpsCamEnt = scene->createFPSCamera("FPSCamera", 60.0f, 1600.0f/900.0f, {0, 5, 20});
    fpsCamEnt->movementSpeed = {8.0f, 5.0f, 15.0f};
    fpsCamEnt->rotationSpeed = {0.15f, 0.15f};
    
    fpsCamEnt.get<CameraComponent>().active = false;

    // Script de switch
    scene->createEntity("CameraManager").add<NativeScriptComponent>([orbitCamEnt, fpsCamEnt, eng = engine.get()](Entity /*ent*/, float /*dt*/) mutable {
        static bool cPressedLastFrame = false;
        auto& input = eng->input();
        bool cIsPressed = input.isKeyPressed(Key::C);

        if (cIsPressed && !cPressedLastFrame) {
            auto& orbitComp = orbitCamEnt.get<CameraComponent>();
            auto& fpsComp = fpsCamEnt.get<CameraComponent>();
            
            orbitComp.active = !orbitComp.active;
            fpsComp.active = !orbitComp.active;

            if (orbitComp.active) {
                BB_CORE_INFO("Switching to Orbit Camera (Left Click: Rotate, Scroll: Zoom)");
            } else {
                BB_CORE_INFO("Switching to FPS Camera (ZQSD: Move, Right Click: Rotate)");
            }
        }
        cPressedLastFrame = cIsPressed;
    });

    // 2. Environnement
    scene->createSkySphere("SkyEnvironment", "assets/textures/skybox_sphere_wood_diffuse.jpeg");

    // 3. Sol
    {
        auto groundMesh = Ref<Mesh>(MeshGenerator::createCheckerboardPlane(engine->graphics(), 200.0f, 40).release());
        auto groundMat = CreateRef<UnlitMaterial>(engine->graphics());
        groundMesh->setMaterial(groundMat);
        scene->createEntity("Ground").add<MeshComponent>(groundMesh);
    }

    // 4. Modèles d'Avions
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

    // --- 5. Fourmi Géante (glTF) ---
    auto ant = scene->createModelEntity("GiantAnt", "assets/models/ant.glb", {0, 2, -80}, {20.0f, 20.0f, 20.0f}); 
    if (ant) {
        ant.setup<TransformComponent>([](auto& t) { 
            t.scale = glm::vec3(1.0f); 
            t.rotation = glm::vec3(0.0f, glm::radians(180.0f), 0.0f);
        });
    }

    // --- 6. Lumières ---
    scene->createDirectionalLight("Sun", {1, 1, 0.9f}, 3.0f, {-45.0f, 45.0f, 0.0f});
    
    scene->createPointLight("RedLight", {1.0f, 0.2f, 0.2f}, 150.0f, 30.0f, {-15.0f, 8.0f, 0.0f});
    scene->createPointLight("GreenLight", {0.2f, 1.0f, 0.2f}, 150.0f, 30.0f, {0.0f, 8.0f, 15.0f});
    scene->createPointLight("BlueLight", {0.2f, 0.2f, 1.0f}, 150.0f, 30.0f, {15.0f, 8.0f, 0.0f});

    BB_CORE_INFO("Demo Engine Ready!");
    engine->Run();

    return 0;
}
