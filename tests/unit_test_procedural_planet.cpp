#include "bb3d/core/Engine.hpp"
#include "bb3d/render/ProceduralMeshGenerator.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/physics/PhysicsWorld.hpp"
#include <iostream>
#include <cassert>

using namespace bb3d;

int main(int argc, char** argv) {
    try {
        auto engine = Engine::Create(Config::Load("config/engine_config.json")
            .title("Visual Test - Procedural Planet")
            .resolution(1280, 720)
            .vsync(true)
            .enableEditor(false)
        );

        auto scene = engine->CreateScene();
        engine->SetActiveScene(scene);

        std::cout << "--- Visual Procedural Planet Test ---" << std::endl;
        std::cout << "Controls: Mouse Left Click to Rotate, Scroll Wheel to Zoom." << std::endl;

        // 1. Create Planet
        auto planetEntity = scene->createEntity("TestPlanet");
        auto& planet = planetEntity.add<ProceduralPlanetComponent>().get<ProceduralPlanetComponent>();
        planet.radius = 10.0f;
        planet.resolution = 192; // Tripled resolution (64 -> 192)
        planet.seed = 9876;
        planet.globalFrequency = 2.0f;
        planet.seaLevel = 10.1f; // Ocean level

        // biomes
        BiomeSettings ocean;
        ocean.name = "Ocean"; ocean.color = {0.1f, 0.2f, 0.7f}; ocean.heightStart = 0.0f; ocean.heightEnd = 0.4f; ocean.amplitude = 0.0f;
        planet.biomes.push_back(ocean);

        BiomeSettings plains;
        plains.name = "Plains"; plains.color = {0.3f, 0.6f, 0.2f}; plains.heightStart = 0.4f; plains.heightEnd = 0.7f; plains.amplitude = 0.5f;
        planet.biomes.push_back(plains);

        BiomeSettings mountains;
        mountains.name = "Mountains"; mountains.color = {0.6f, 0.5f, 0.4f}; mountains.heightStart = 0.7f; mountains.heightEnd = 1.0f; mountains.amplitude = 2.0f;
        planet.biomes.push_back(mountains);

        // Pre-rebuild model
        planet.model = ProceduralMeshGenerator::createPlanet(engine->graphics(), engine->assets(), engine->jobs(), planet);
        auto pbr = CreateRef<PBRMaterial>(engine->graphics());
        pbr->setColor({1.0f, 1.0f, 1.0f});
        for (auto& mesh : planet.model->getMeshes()) {
            mesh->setMaterial(pbr);
        }
        planet.needsRebuild = false;
        planetEntity.add<ModelComponent>(planet.model);
        
        // 1a. Physics for Planet (Static Mesh)
        auto& planetPhys = planetEntity.add<PhysicsComponent>().get<PhysicsComponent>();
        planetPhys.type = BodyType::Static;
        planetPhys.colliderType = ColliderType::Mesh;
        planetPhys.useModelMesh = true;
        
        // 1b. Create Moon
        auto moonEntity = scene->createEntity("Moon");
        auto& moon = moonEntity.add<ProceduralPlanetComponent>().get<ProceduralPlanetComponent>();
        moon.radius = 2.5f;
        moon.resolution = 64; 
        moon.seed = 1234;
        moon.globalFrequency = 3.0f;

        BiomeSettings crater;
        crater.name = "Craters"; crater.color = {0.5f, 0.5f, 0.5f}; crater.heightStart = 0.0f; crater.heightEnd = 1.0f; crater.amplitude = 0.3f;
        moon.biomes.push_back(crater);

        moon.model = ProceduralMeshGenerator::createPlanet(engine->graphics(), engine->assets(), engine->jobs(), moon);
        auto moonMat = CreateRef<PBRMaterial>(engine->graphics());
        moonMat->setColor({0.8f, 0.8f, 0.8f});
        for (auto& mesh : moon.model->getMeshes()) {
            mesh->setMaterial(moonMat);
        }
        moon.needsRebuild = false;
        moonEntity.add<ModelComponent>(moon.model);
        moonEntity.get<TransformComponent>().translation = {25.0f, 0.0f, 0.0f};

        // Moon Orbit Behavior (Script)
        moonEntity.add<NativeScriptComponent>([&](Entity entity, float dt) {
            static float time = 0.0f;
            time += dt;
            float orbitRadius = 25.0f;
            float orbitSpeed = 0.5f;
            auto& tc = entity.get<TransformComponent>();
            tc.translation = {
                std::cos(time * orbitSpeed) * orbitRadius,
                std::sin(time * orbitSpeed * 0.5f) * 5.0f, 
                std::sin(time * orbitSpeed) * orbitRadius
            };
        });

        // 1c. Create Asteroids with Gravity
        engine->physics().setGravity({0, 0, 0}); // Disable global gravity for planetary space
        auto asteroidMat = CreateRef<PBRMaterial>(engine->graphics());
        asteroidMat->setColor({0.4f, 0.35f, 0.3f});
        PBRParameters asteroidParams;
        asteroidParams.roughnessFactor = 0.9f;
        asteroidMat->setParameters(asteroidParams);

        for (int i = 0; i < 20; i++) {
            auto asteroid = scene->createEntity("Asteroid_" + std::to_string(i));
            
            // Random position in a shell around the planet
            float radius = 15.0f + (rand() % 100) / 10.0f;
            float angle = (rand() % 360) * glm::pi<float>() / 180.0f;
            float height = ((rand() % 100) / 50.0f - 1.0f) * 5.0f;
            asteroid.get<TransformComponent>().translation = {
                std::cos(angle) * radius,
                height,
                std::sin(angle) * radius
            };
            asteroid.get<TransformComponent>().scale = glm::vec3(0.3f + (rand() % 10) / 20.0f);

            // Asteroid Physics (Dynamic Sphere)
            auto& phys = asteroid.add<PhysicsComponent>().get<PhysicsComponent>();
            phys.type = BodyType::Dynamic;
            phys.colliderType = ColliderType::Sphere;
            phys.radius = 0.5f;
            phys.mass = 1.0f;
            phys.useModelMesh = false; // Keep it as a fast sphere for physics
            
            // Initial orbital velocity (approximate)
            glm::vec3 pos = asteroid.get<TransformComponent>().translation;
            glm::vec3 toCenter = -glm::normalize(pos);
            glm::vec3 tangent = glm::normalize(glm::cross(toCenter, {0, 1, 0}));
            phys.initialLinearVelocity = tangent * 5.0f + glm::vec3(0, (rand() % 10) / 5.0f - 1.0f, 0);

            // Asteroid gravity script
            asteroid.add<NativeScriptComponent>([&, planetPos = glm::vec3(0.0f)](Entity entity, float dt) {
                auto& tc = entity.get<TransformComponent>();
                glm::vec3 direction = planetPos - tc.translation;
                float distanceSq = glm::dot(direction, direction);
                if (distanceSq < 1.0f) return; // Avoid singularity
                
                glm::vec3 force = glm::normalize(direction) * (500.0f / distanceSq);
                engine->physics().addForce(entity, force);
            });

            // Asteroid Visual (Low-poly Procedural Planet)
            auto& astPlanet = asteroid.add<ProceduralPlanetComponent>().get<ProceduralPlanetComponent>();
            astPlanet.radius = 0.5f;
            astPlanet.resolution = 8; // Low poly for performance
            astPlanet.seed = 42 + i;
            astPlanet.globalFrequency = 5.0f;
            BiomeSettings astBiome;
            astBiome.name = "Rock"; astBiome.color = {0.4f, 0.35f, 0.3f}; 
            astBiome.heightStart = 0.0f; astBiome.heightEnd = 1.0f; astBiome.amplitude = 0.15f;
            astPlanet.biomes.push_back(astBiome);

            astPlanet.model = ProceduralMeshGenerator::createPlanet(engine->graphics(), engine->assets(), engine->jobs(), astPlanet);
            for (auto& mesh : astPlanet.model->getMeshes()) {
                mesh->setMaterial(asteroidMat);
            }
            astPlanet.needsRebuild = false;
            asteroid.add<ModelComponent>(astPlanet.model);
        }

        // 2. Camera: Orbit Controller
        scene->createOrbitCamera("OrbitCam", 60.0f, 1280.0f/720.0f, {0,0,0}, 40.0f);

        // 3. Lighting
        scene->createDirectionalLight("Sun", {1.0f, 1.0f, 0.9f}, 4.0f, {0.5f, -0.8f, 0.0f});
        scene->createSkySphere("Stars", "assets/textures/skybox_sphere/ringed_gas_giant_planet.png");

        engine->Run();

        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "CRASH: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
