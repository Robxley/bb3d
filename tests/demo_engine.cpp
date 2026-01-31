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
        
                // 1. Caméra
                auto cameraEntity = scene->createOrbitCamera("MainCamera", 45.0f, 1600.0f/900.0f, {0, 2, 0}, 30.0f);
        
                // 2. Environnement
                scene->createSkySphere("SkyEnvironment", "assets/textures/skybox_sphere_wood_diffuse.jpeg");
        
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
                    auto e = scene->createModelEntity("Plane", path, {(float)(rand()%40 - 20), 5.0f, (float)(rand()%40 - 20)}, {10.0f, 10.0f, 10.0f});
                    if (e) {
                        e.add<NativeScriptComponent>([](Entity ent, float dt) {
                            auto& t = ent.get<TransformComponent>();
                            glm::quat rotationStep = glm::angleAxis((float)(dt * 1.5f), glm::vec3(0.0f, 1.0f, 0.0f));
                            t.rotation = rotationStep * t.rotation;
                        });
                    }
                }
        
                // --- 5. Fourmi Géante (glTF) ---
                auto ant = scene->createModelEntity("GiantAnt", "assets/models/ant.glb", {0, 2, -40}, {20.0f, 20.0f, 20.0f}); 
                if (ant) {
                    ant.setup<TransformComponent>([](auto& t) { 
                        t.scale = glm::vec3(1.0f); 
                        t.rotation = glm::vec3(0.0f, glm::radians(180.0f), 0.0f);
                    });
                }
        
                // --- 6. Lumières ---
                scene->createDirectionalLight("Sun", {1, 1, 0.9f}, 10.0f, {-45.0f, 45.0f, 0.0f});
        
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