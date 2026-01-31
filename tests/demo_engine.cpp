#include "bb3d/core/Engine.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/scene/OrbitCamera.hpp"
#include "bb3d/render/MeshGenerator.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

using namespace bb3d;

int main() {
    Scope<Engine> engine = nullptr;
    {
        engine = Engine::Create(EngineConfig()
            .title("biobazard3d - Ultimate Engine Demo")
            .resolution(1600, 900)
            .vsync(true)
        );

        auto scene = engine->CreateScene();
        engine->SetActiveScene(scene);

        auto cameraEntity = scene->createEntity("MainCamera");
        auto orbitCam = CreateRef<OrbitCamera>(45.0f, 1600.0f/900.0f, 0.1f, 2000.0f);
        orbitCam->setTarget({0, 2, 0});
        orbitCam->zoom(-30.0f);
        cameraEntity.add<CameraComponent>(orbitCam);

        cameraEntity.add<NativeScriptComponent>([eng = engine.get()](Entity entity, float dt) {
            auto& camComp = entity.get<CameraComponent>();
            auto* orbit = dynamic_cast<OrbitCamera*>(camComp.camera.get());
            if (!orbit) return;
            auto& input = eng->input();
            if (input.isMouseButtonPressed(Mouse::Left)) {
                glm::vec2 delta = input.getMouseDelta();
                orbit->rotate(delta.x * 5.0f, -delta.y * 5.0f);
            }
            orbit->zoom(input.getMouseScroll().y * 2.0f);
        });

        auto& assets = engine->assets();

        // 2. Environnement
        {
            auto skyTex = assets.load<Texture>("assets/textures/skybox_sphere_wood_diffuse.jpeg", true);
            scene->createEntity("SkyEnvironment").add<SkySphereComponent>(skyTex);
        }

        // 3. Sol
        {
            auto groundMesh = Ref<Mesh>(MeshGenerator::createCheckerboardPlane(engine->graphics(), 200.0f, 40).release());
            scene->createEntity("Ground").add<MeshComponent>(groundMesh);
        }

        // 4. Modèles d'Avions
        std::vector<std::string> planePaths = {
            "assets/models/planes/plane01/Plane01.obj",
            "assets/models/planes/plane02/Plane02.obj",
            "assets/models/planes/plane03/Plane_03.obj",
            "assets/models/planes/plane05/Plane05.obj",
            "assets/models/planes/plane06/Plane_06.obj"
        };

        for (const auto& path : planePaths) {
            try {
                auto model = assets.load<Model>(path);
                model->normalize({10.0f, 10.0f, 10.0f}); // Ensure consistent size
                auto e = scene->createEntity("Plane").at({(float)(rand()%40 - 20), 5.0f, (float)(rand()%40 - 20)});
                e.add<ModelComponent>(model);
                
                e.add<NativeScriptComponent>([](Entity ent, float dt) {
                    auto& t = ent.get<TransformComponent>();
                    glm::quat rotationStep = glm::angleAxis((float)(dt * 1.5f), glm::vec3(0.0f, 1.0f, 0.0f));
                    t.rotation = rotationStep * t.rotation;
                });
            } catch(...) {}
        }

        // --- 5. Fourmi Géante (glTF) ---
        try {
            auto antModel = assets.load<Model>("assets/models/ant.glb");
            antModel->normalize({20.0f, 20.0f, 20.0f}); 
            scene->createEntity("GiantAnt")
                .at({0, 2, -40})
                .add<ModelComponent>(antModel)
                .setup<TransformComponent>([](auto& t) { 
                    t.scale = glm::vec3(1.0f); 
                    t.rotation = glm::vec3(0.0f, glm::radians(180.0f), 0.0f);
                });
        } catch(...) {}

        // --- 6. Lumières ---
        scene->createEntity("Sun")
            .add<LightComponent>(LightType::Directional, glm::vec3(1, 1, 0.9f), 10.0f)
            .setup<TransformComponent>([](auto& t) {
                t.rotation = glm::vec3(glm::radians(-45.0f), glm::radians(45.0f), 0.0f);
            });

        BB_CORE_INFO("Demo Engine Ready!");
        engine->Run();

        // NETTOYAGE BRUTAL
        BB_CORE_INFO("Demo: Dropping all local references...");
        scene.reset();
    } 

    BB_CORE_INFO("Demo: Shutting down engine...");
    engine->Shutdown();
    engine.reset();

    return 0;
}