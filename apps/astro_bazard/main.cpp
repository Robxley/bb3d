#include "bb3d/core/Engine.hpp"
#include "bb3d/physics/PhysicsWorld.hpp"
#include "bb3d/scene/Scene.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/render/MeshGenerator.hpp"
#include "bb3d/render/ProceduralMeshGenerator.hpp"
#include "OrbitalGravity.hpp"
#include "SpaceshipController.hpp"
#include "SmartCamera.hpp"
#include <iostream>
#include <algorithm>

using namespace bb3d;

int main(int argc, char** argv) {
    try {
        bool enableEditor = false;
#if defined(BB3D_DEBUG)
        enableEditor = true;
#endif

        auto engine = Engine::Create(Config::Load("config/engine_config.json")
            .title("AstroBazard - Kerbal Space 2D")
            .resolution(1280, 720)
            .vsync(true)
            .enablePhysics(PhysicsBackend::Jolt)
            .enableOffscreenRendering(true) // Always render to texture for post-processing/editor
            .enableEditor(enableEditor)
        );

        {
            auto scene = engine->CreateScene();
            engine->SetActiveScene(scene);
            
            // Disable global gravity for orbital simulation
            engine->physics().setGravity({0.0f, 0.0f, 0.0f});

            // 1. Create Procedural Planet
            auto planet = scene->createEntity("Planet");
            planet.get<TransformComponent>().translation = {0.0f, 0.0f, 0.0f}; // Center of the system
            
            auto& planetComp = planet.add<ProceduralPlanetComponent>().get<ProceduralPlanetComponent>();
            planetComp.radius = 10.0f;
            planetComp.resolution = 128; 
            planetComp.seed = 4242;
            planetComp.globalFrequency = 2.0f;
            planetComp.seaLevel = 10.1f;
            
            // Define Biomes
            BiomeSettings ocean;
            ocean.name = "Deep Ocean"; ocean.color = {0.05f, 0.15f, 0.4f}; ocean.heightStart = 0.0f; ocean.heightEnd = 0.35f; ocean.amplitude = 0.0f; 
            planetComp.biomes.push_back(ocean);

            BiomeSettings plains;
            plains.name = "Green Plains"; plains.color = {0.2f, 0.5f, 0.1f}; plains.heightStart = 0.35f; plains.heightEnd = 0.7f; plains.amplitude = 0.5f;
            planetComp.biomes.push_back(plains);

            BiomeSettings mountains;
            mountains.name = "Rocky Mountains"; mountains.color = {0.6f, 0.5f, 0.4f}; mountains.heightStart = 0.7f; mountains.heightEnd = 1.0f; mountains.amplitude = 2.0f;
            planetComp.biomes.push_back(mountains);

            // Build Planet Mesh
            planetComp.model = ProceduralMeshGenerator::createPlanet(engine->graphics(), engine->assets(), engine->jobs(), planetComp);
            auto planetMat = CreateRef<PBRMaterial>(engine->graphics());
            planetMat->setColor({1.0f, 1.0f, 1.0f});
            for (auto& mesh : planetComp.model->getMeshes()) {
                mesh->setMaterial(planetMat);
            }
            planetComp.needsRebuild = false;
            planet.add<ModelComponent>(planetComp.model);

            auto& planetPhys = planet.add<PhysicsComponent>().get<PhysicsComponent>();
            planetPhys.type = BodyType::Static;
            planetPhys.colliderType = ColliderType::Mesh; 
            planetPhys.useModelMesh = true; // Use the procedural mesh as collision shape
            engine->physics().createRigidBody(planet);

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

            // 1c. Create Asteroids
            auto asteroidMat = CreateRef<PBRMaterial>(engine->graphics());
            asteroidMat->setColor({0.4f, 0.35f, 0.3f});
            for (int i = 0; i < 20; i++) {
                auto asteroid = scene->createEntity("Asteroid_" + std::to_string(i));
                float radius = 15.0f + (rand() % 100) / 10.0f;
                float angle = (rand() % 360) * glm::pi<float>() / 180.0f;
                asteroid.get<TransformComponent>().translation = { std::cos(angle) * radius, 0.0f, std::sin(angle) * radius };
                asteroid.get<TransformComponent>().scale = glm::vec3(0.3f + (rand() % 10) / 20.0f);

                auto& phys = asteroid.add<PhysicsComponent>().get<PhysicsComponent>();
                phys.type = BodyType::Dynamic;
                phys.colliderType = ColliderType::Sphere;
                phys.radius = 0.5f;
                phys.mass = 1.0f;
                phys.initialLinearVelocity = glm::vec3(std::cos(angle+1.5f), 0.0f, std::sin(angle+1.5f)) * 5.0f;
                engine->physics().createRigidBody(asteroid);

                // Asteroid Visual
                auto& astPlanet = asteroid.add<ProceduralPlanetComponent>().get<ProceduralPlanetComponent>();
                astPlanet.radius = 0.5f; astPlanet.resolution = 8; astPlanet.seed = 42 + i;
                BiomeSettings astBiome; astBiome.name = "Rock"; astBiome.color = {0.4f, 0.35f, 0.3f}; astBiome.heightStart = 0.0f; astBiome.heightEnd = 1.0f; astBiome.amplitude = 0.15f;
                astPlanet.biomes.push_back(astBiome);
                astPlanet.model = ProceduralMeshGenerator::createPlanet(engine->graphics(), engine->assets(), engine->jobs(), astPlanet);
                for (auto& mesh : astPlanet.model->getMeshes()) mesh->setMaterial(asteroidMat);
                asteroid.add<ModelComponent>(astPlanet.model);

                // Radial Gravity
                asteroid.add<NativeScriptComponent>([enginePtr = engine.get(), planet](Entity entity, float dt) mutable {
                    auto& tc = entity.get<TransformComponent>();
                    glm::vec3 direction = planet.get<TransformComponent>().translation - tc.translation;
                    float distanceSq = glm::dot(direction, direction);
                    if (distanceSq > 1.0f) {
                        glm::vec3 force = glm::normalize(direction) * (500.0f / distanceSq);
                        enginePtr->physics().addForce(entity, force);
                    }
                });
            }

            // Environment
            scene->createSkySphere("Cosmos", "assets/textures/skybox_sphere/ringed_gas_giant_planet.png");
            
            // Sun
            scene->createDirectionalLight("Sun", {1.0f, 0.95f, 0.8f}, 5.0f, {0.8f, -0.6f, 0.0f});

            // Spaceship Assembly (Player Ship)
            auto shipModel = engine->assets().load<Model>("assets/models/rockets/rocket_2/scene.gltf");
            shipModel->normalize(glm::vec3(1.0f)); 

            auto ship = scene->createEntity("Spaceship");
            ship.get<TransformComponent>().translation = {0.0f, 11.0f, 0.0f}; 
            
            // Visual Entity (Manual Sync because no Hierarchy in bb3d yet)
            auto shipVisual = scene->createEntity("SpaceshipVisual");
            auto& modelComp = shipVisual.add<ModelComponent>().get<ModelComponent>();
            modelComp.model = shipModel;

            auto& phys = ship.add<PhysicsComponent>().get<PhysicsComponent>();
            phys.type = BodyType::Dynamic;
            phys.mass = 5.0f;
            phys.friction = 0.5f;
            phys.colliderType = ColliderType::Box;
            phys.boxHalfExtents = {0.2f, 0.5f, 0.2f}; 
            phys.constrain2D = true;
            engine->physics().createRigidBody(ship);

            // Gameplay components
            ship.add<SpaceshipControllerComponent>();
            ship.add<OrbitalGravityComponent>(500.0f, static_cast<uint32_t>(planet.getHandle()));

            // Particles for propulsion
            auto& ps = ship.add<ParticleSystemComponent>().get<ParticleSystemComponent>();
            auto fireTex = engine->assets().load<Texture>("assets/textures/particles/fire.png", true);
            auto particleMat = CreateRef<UnlitMaterial>(engine->graphics());
            if (fireTex) particleMat->setBaseMap(fireTex);
            particleMat->setColor({2.0f, 1.0f, 0.5f});
            ps.material = particleMat;

            auto shipNose = scene->createEntity("SpaceshipNose");

            // Camera 1: Spaceship Follow (SmartCamera)
            auto shipCamera = scene->createEntity("ShipCamera");
            auto& camComp = shipCamera.add<CameraComponent>().get<CameraComponent>();
            camComp.fov = 60.0f;
            camComp.nearPlane = 0.1f;
            camComp.farPlane = 20000.0f;
            camComp.active = true;
            camComp.camera = CreateRef<Camera>(60.0f, 1280.0f/720.0f, 0.1f, 20000.0f);
            shipCamera.get<TransformComponent>().translation = {0.0f, 2.0f, 20.0f};

            // Camera 2: Planet Orbit (World View)
            auto planetOrbitView = scene->createOrbitCamera("PlanetOrbitCamera", 60.0f, 1280.0f/720.0f, {0,0,0}, 50.0f);
            planetOrbitView.get<CameraComponent>().active = false; // Start inactive

            // Planetary System Logic & Smart Camera
            auto orbitalGravity = std::make_shared<astrobazard::OrbitalGravity>(planet); 
            auto spaceshipCtrl = std::make_shared<astrobazard::SpaceshipController>(shipNose);
            auto smartCamera = std::make_shared<astrobazard::SmartCamera>(shipCamera, planet, 10.0f);

            scene->createEntity("KerbalLogic").add<NativeScriptComponent>([enginePtr = engine.get(), ship, shipVisual, planet, shipCamera, planetOrbitView, orbitalGravity, spaceshipCtrl, smartCamera](Entity e, float dt) mutable {
                auto& input = enginePtr->input();

                // 0. Toggle Camera (TAB)
                static bool tabWasPressed = false;
                bool tabIsPressed = input.isKeyPressed(Key::Tab);
                if (tabIsPressed && !tabWasPressed) {
                    auto& shipCam = shipCamera.get<CameraComponent>();
                    auto& orbitCam = planetOrbitView.get<CameraComponent>();
                    shipCam.active = !shipCam.active;
                    orbitCam.active = !shipCam.active;
                    BB_CORE_INFO("AstroBazard: Switched camera to {0}", shipCam.active ? "Spaceship Follow" : "Planet Orbit");
                }
                tabWasPressed = tabIsPressed;

                // 1. Newton Gravity
                orbitalGravity->update(ship, enginePtr);
                
                // 2. Spaceship Controls in 2D Space
                spaceshipCtrl->update(ship, dt, enginePtr);

                // 3. Manual Sync Visual Model with Physics Body
                if (ship.has<TransformComponent>() && shipVisual.has<TransformComponent>()) {
                    auto& stf = ship.get<TransformComponent>();
                    auto& vtf = shipVisual.get<TransformComponent>();
                    
                    // The Physics BoxCollider has halfExtents of 0.5 on Y. If the visual origin is at the bottom, 
                    // we need to offset the visual model downwards by halfExtents so its bottom aligns with the collider's bottom.
                    glm::vec3 shipLocalDown = {std::sin(stf.rotation.z), -std::cos(stf.rotation.z), 0.0f};

                    vtf.translation = stf.translation + shipLocalDown * 0.45f;
                    vtf.rotation = stf.rotation;
                    // Removed the hardcoded -90 on X so the longest part (Y axis) aligns with propulsion
                    // Base scale for the rocket
                    float baseScale = 0.15f; 
                    vtf.scale = stf.scale * baseScale;
                }

                // 4. Update Cameras
                if (shipCamera.get<CameraComponent>().active) {
                    // Update Smart Camera
                    auto& tf = ship.get<TransformComponent>();
                    glm::vec3 planetCenter = planet.get<TransformComponent>().translation;
                    float dist = glm::length(tf.translation - planetCenter);
                    float altitude = dist - 10.0f;

                    float scroll = input.getMouseScroll().y;
                    if (std::abs(scroll) > 0.01f) smartCamera->zoom(scroll);

                    float scaleFactor = smartCamera->update(ship, altitude, orbitalGravity->getCurrentGravityDirection());
                    
                    // 5. Illusion: dynamic visual scale
                    spaceshipCtrl->applyVisualScale(ship, scaleFactor);
                } else {
                    // Orbit camera is updated automatically by the Scene system
                    // But we might want to ensure the spaceship doesn't "disappear" or stays at a reasonable scale
                    spaceshipCtrl->applyVisualScale(ship, 1.0f); 
                }
            });

            engine->Run();

            if (engine->graphics().getDevice()) {
                engine->graphics().getDevice().waitIdle();
            }
            engine->SetActiveScene(nullptr);
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
