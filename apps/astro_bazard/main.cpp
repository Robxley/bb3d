#include "bb3d/core/Engine.hpp"
#include "bb3d/physics/PhysicsWorld.hpp"
#include "bb3d/scene/Scene.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/render/MeshGenerator.hpp"
#include "OrbitalGravity.hpp"
#include "SpaceshipController.hpp"
#include "SmartCamera.hpp"
#include <iostream>
#include <algorithm>

using namespace bb3d;

int main(int argc, char** argv) {
    try {
        auto engine = Engine::Create(Config::Load("config/engine_config.json")
            .title("AstroBazard - Kerbal Space 2D")
            .resolution(1280, 720)
            .vsync(true)
            .enablePhysics(PhysicsBackend::Jolt)
            .enableEditor(false)
        );

        {
            auto scene = engine->CreateScene();
            engine->SetActiveScene(scene);
            
            // Disable global gravity for orbital simulation
            engine->physics().setGravity({0.0f, 0.0f, 0.0f});

            // Massive Procedural Planet
            auto planet = scene->createEntity("Planet");
            planet.get<TransformComponent>().translation = {0.0f, -1000.0f, 0.0f};
            
            auto& planetComp = planet.add<ProceduralPlanetComponent>().get<ProceduralPlanetComponent>();
            planetComp.radius = 1000.0f;
            planetComp.resolution = 127; // Use 127 for ODD number of vertices (128) if needed, otherwise 128
            planetComp.seed = 4242;
            planetComp.globalFrequency = 0.002f;
            
            // Define Biomes
            BiomeSettings ocean;
            ocean.name = "Deep Ocean";
            ocean.color = {0.05f, 0.15f, 0.4f};
            ocean.heightStart = 0.0f;
            ocean.heightEnd = 0.35f;
            ocean.amplitude = 0.0f; 
            planetComp.biomes.push_back(ocean);

            BiomeSettings plains;
            plains.name = "Green Plains";
            plains.color = {0.2f, 0.5f, 0.1f};
            plains.heightStart = 0.35f;
            plains.heightEnd = 0.7f;
            plains.frequency = 0.015f;
            plains.amplitude = 12.0f;
            planetComp.biomes.push_back(plains);

            BiomeSettings mountains;
            mountains.name = "Rocky Mountains";
            mountains.color = {0.5f, 0.45f, 0.4f};
            mountains.heightStart = 0.7f;
            mountains.heightEnd = 1.0f;
            mountains.frequency = 0.04f;
            mountains.amplitude = 45.0f;
            planetComp.biomes.push_back(mountains);

            auto& planetPhys = planet.add<PhysicsComponent>().get<PhysicsComponent>();
            planetPhys.type = BodyType::Static;
            planetPhys.colliderType = ColliderType::Mesh; 
            planetPhys.useModelMesh = false;

            // Environment
            scene->createSkySphere("Cosmos", "assets/textures/skybox_sphere/ringed_gas_giant_planet.png");
            
            // Sun
            scene->createDirectionalLight("Sun", {1.0f, 0.95f, 0.8f}, 5.0f, {0.8f, -0.6f, 0.0f});

            // Spaceship Assembly (Kenney 3D Model)
            auto shipModel = engine->assets().load<Model>("assets/models/kenney_space-kit/Models/OBJ format/craft_speederA.obj");

            auto ship = scene->createEntity("Spaceship");
            auto& modelComp = ship.add<ModelComponent>().get<ModelComponent>();
            modelComp.model = shipModel;
            ship.get<TransformComponent>().translation = {0.0f, 1.0f, 0.0f}; // Just above the surface
            
            auto& phys = ship.add<PhysicsComponent>().get<PhysicsComponent>();
            phys.type = BodyType::Dynamic;
            phys.mass = 5.0f;
            phys.friction = 0.5f;
            phys.colliderType = ColliderType::Box;
            phys.boxHalfExtents = {0.5f, 0.5f, 0.5f};
            phys.constrain2D = true; // Stay on the 2D plane!
            engine->physics().createRigidBody(ship);

            // Particles for propulsion
            auto& ps = ship.add<ParticleSystemComponent>().get<ParticleSystemComponent>();
            auto fireTex = engine->assets().load<Texture>("assets/textures/particles/fire.png", true);
            auto particleMat = CreateRef<UnlitMaterial>(engine->graphics());
            if (fireTex) particleMat->setBaseMap(fireTex);
            particleMat->setColor({2.0f, 1.0f, 0.5f}); // Fire tint
            ps.material = particleMat;

            // The nose is no longer a separate visual mesh, but we keep the entity for the Controller logic
            auto shipNose = scene->createEntity("SpaceshipNose");

            // Camera
            auto camera = scene->createEntity("MainCamera");
            auto& camComp = camera.add<CameraComponent>().get<CameraComponent>();
            camComp.fov = 60.0f;
            camComp.nearPlane = 0.1f;
            camComp.farPlane = 20000.0f;
            camComp.active = true;
            camComp.camera = CreateRef<Camera>(60.0f, 1280.0f/720.0f, 0.1f, 20000.0f);
            camera.get<TransformComponent>().translation = {0.0f, 2.0f, 20.0f};

            // Planetary System Logic & Smart Camera
            auto orbitalGravity = std::make_shared<astrobazard::OrbitalGravity>(planet, 500000.0f);
            auto spaceshipCtrl = std::make_shared<astrobazard::SpaceshipController>(shipNose, 250.0f, 100.0f, 20.0f);
            auto smartCamera = std::make_shared<astrobazard::SmartCamera>(camera, planet, 20.0f);

            scene->createEntity("KerbalLogic").add<NativeScriptComponent>([enginePtr = engine.get(), ship, planet, camera, orbitalGravity, spaceshipCtrl, smartCamera](Entity e, float dt) mutable {
                // 1. Newton Gravity
                orbitalGravity->update(ship, enginePtr);
                
                // 2. Spaceship Controls in 2D Space
                spaceshipCtrl->update(ship, dt, enginePtr);

                // Calculate altitude for camera
                auto& tf = ship.get<TransformComponent>();
                glm::vec3 planetCenter = planet.get<TransformComponent>().translation;
                float dist = glm::length(tf.translation - planetCenter);
                float altitude = dist - 1000.0f;

                // 3. Smart Camera updates and gives us the visual scale factor
                float scroll = enginePtr->input().getMouseScroll().y;
                if (std::abs(scroll) > 0.01f) smartCamera->zoom(scroll);

                float scaleFactor = smartCamera->update(ship, altitude, orbitalGravity->getCurrentGravityDirection());

                // 4. Illusion: dynamic visual scale
                spaceshipCtrl->applyVisualScale(ship, scaleFactor);
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
