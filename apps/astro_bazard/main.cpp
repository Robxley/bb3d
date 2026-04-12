#include "bb3d/core/Engine.hpp"
#include "bb3d/physics/PhysicsWorld.hpp"
#include "bb3d/scene/Scene.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/render/MeshGenerator.hpp"
#include "bb3d/render/ProceduralMeshGenerator.hpp"
#include "SpaceshipController.hpp"
#include "SmartCamera.hpp"
#include "CommRelay.hpp"
#include <iostream>
#include <algorithm>
#include <imgui.h>

#include "bb3d/scene/ComponentRegistry.hpp"

using namespace bb3d;

struct AstroBazardStateComponent {
    bool tabWasPressed = false;
    bool cWasPressed = false;
    // Cached entities for O(1) access every frame
    Entity ship;
    Entity planet;
    Entity moon;
    Entity shipCamera;
    Entity planetOrbitView;
    
    // Controller state
    std::shared_ptr<astrobazard::SpaceshipController> shipCtrl;
    std::shared_ptr<astrobazard::SmartCamera> smartCamera;

    void serialize(nlohmann::json& j) const {}
    void deserialize(const nlohmann::json& j) {}
};

struct SimpleTimeComponent {
    float time = 0.0f;
    void serialize(nlohmann::json& j) const { j["time"] = time; }
    void deserialize(const nlohmann::json& j) { time = j.value("time", 0.0f); }
};

static void AstroBazardLogic(Entity e, float dt) {
    Engine& engine = Engine::Get();
    auto scene = engine.GetActiveScene();
    if (!scene) return;

    // Ensure state component exists on the logic entity
    if (!e.has<AstroBazardStateComponent>()) {
        e.add<AstroBazardStateComponent>();
    }
    auto& state = e.get<AstroBazardStateComponent>();

    // Update/Cache entities if invalid
    if (!state.ship || !state.ship.isValid()) state.ship = scene->findEntityByName("Spaceship");
    if (!state.planet || !state.planet.isValid()) state.planet = scene->findEntityByName("Planet");
    if (!state.moon || !state.moon.isValid()) state.moon = scene->findEntityByName("Moon");
    if (!state.shipCamera || !state.shipCamera.isValid()) state.shipCamera = scene->findEntityByName("ShipCamera");
    if (!state.planetOrbitView || !state.planetOrbitView.isValid()) state.planetOrbitView = scene->findEntityByName("PlanetOrbitCamera");

    auto ship = state.ship;
    auto planet = state.planet;
    auto moonEntity = state.moon;
    auto shipCamera = state.shipCamera;
    auto planetOrbitView = state.planetOrbitView;

    if (!ship || !planet || !shipCamera) return;

    // Re-bind if stale or uninitialized (e.g. after a Save/Load)
    if (!state.smartCamera || !state.smartCamera->getTarget().isValid() || !state.smartCamera->getCamera().isValid() || state.smartCamera->getCamera() != shipCamera) {
        state.smartCamera = std::make_shared<astrobazard::SmartCamera>(shipCamera, ship, 20.0f);
    }
    if (!state.shipCtrl) {
        state.shipCtrl = std::make_shared<astrobazard::SpaceshipController>();
    }

    auto& input = engine.input();
    if (input.isKeyPressed(Key::R)) {
        engine.importScene("astro_bazard_scene.json");
        return;
    }

    // 0. Toggle Camera (TAB)
    bool tabIsPressed = input.isKeyPressed(Key::Tab);
    if (tabIsPressed && !state.tabWasPressed) {
        auto& shipCam = shipCamera.get<CameraComponent>();
        auto& orbitCam = planetOrbitView.get<CameraComponent>();
        auto& smartCam = shipCamera.get<bb3d::SmartCameraComponent>();
        
        if (orbitCam.active) {
            // Planet -> Ship (Mode 0)
            orbitCam.active = false;
            shipCam.active = true;
            smartCam.mode = 0;
            state.smartCamera->resetSmoothing();
            state.smartCamera->setTarget(ship);
            BB_CORE_INFO("AstroBazard: Mode Caméra : Dolly Zoom (FOV)");
        } else {
            if (smartCam.mode == 0) {
                smartCam.mode = 1;
                BB_CORE_INFO("AstroBazard: Mode Caméra : Réalisme Pur (Pas d'aide visuelle)");
            } else if (smartCam.mode == 1) {
                smartCam.mode = 2;
                BB_CORE_INFO("AstroBazard: Mode Caméra : Visual Scale (KSP Style)");
            } else {
                // Ship -> Planet
                shipCam.active = false;
                orbitCam.active = true;
                auto& orbitCtrl = planetOrbitView.get<OrbitControllerComponent>();
                orbitCtrl.target = glm::vec3(0.0f, 0.0f, 0.0f);
                BB_CORE_INFO("AstroBazard: Mode Caméra : Orbite Planétaire");
            }
        }
    }
    state.tabWasPressed = tabIsPressed;

    // 0b. COMM-LINK Camera Switch (C key)
    bool cIsPressed = input.isKeyPressed(Key::C);
    if (cIsPressed && !state.cWasPressed && shipCamera.get<CameraComponent>().active) {
        auto view = scene->getRegistry().view<astrobazard::CommRelayComponent>();
        std::vector<bb3d::Entity> targets;
        for (auto entityID : view) targets.push_back(bb3d::Entity(entityID, *scene));
        
        if (!targets.empty()) {
            auto currentTarget = state.smartCamera->getTarget();
            auto it = std::find(targets.begin(), targets.end(), currentTarget);
            if (it == targets.end()) it = targets.begin();
            else {
                it++;
                if (it == targets.end()) it = targets.begin();
            }
            
            if (*it != currentTarget) {
                state.smartCamera->setTarget(*it);
                BB_CORE_INFO("COMM-LINK: Basculé sur {}", (*it).get<astrobazard::CommRelayComponent>().name);
            }
        }
    }
    state.cWasPressed = cIsPressed;

    // 1. Scene Controls (S=Save, Backspace=Reset Ship)
    if (input.isKeyPressed(Key::S)) {
        engine.exportScene("astro_bazard_scene.json");
    }
    if (input.isKeyJustPressed(Key::Backspace)) {
        auto& tf = ship.get<TransformComponent>();
        tf.translation = {0.0f, 15.0f, 0.0f};
        tf.rotation = {0.0f, 0.0f, 0.0f};
        auto& phys = ship.get<PhysicsComponent>();
        phys.initialLinearVelocity = {0.0f, 0.0f, 0.0f};
        engine.physics().resetBody(ship);
        BB_CORE_INFO("AstroBazard: Spaceship Reset!");
    }

    // 2. Application de la Gravité Planétaire (PointGravitySourceComponent)
    auto gravitySources = scene->getRegistry().view<bb3d::PointGravitySourceComponent, bb3d::TransformComponent>();
    auto dynamicBodies = scene->getRegistry().view<bb3d::PhysicsComponent, bb3d::TransformComponent>();

    for (auto bodyEntityID : dynamicBodies) {
        auto [phys, bodyTf] = dynamicBodies.get<bb3d::PhysicsComponent, bb3d::TransformComponent>(bodyEntityID);
        if (phys.type != bb3d::BodyType::Dynamic) continue;

        bb3d::Entity bodyEntity{bodyEntityID, *scene};
        glm::vec3 totalForce{0.0f};

        for (auto gravEntityID : gravitySources) {
            if (gravEntityID == bodyEntityID) continue; // Un corps ne s'attire pas lui-même
            auto [grav, gravTf] = gravitySources.get<bb3d::PointGravitySourceComponent, bb3d::TransformComponent>(gravEntityID);
            
            glm::vec3 direction = gravTf.translation - bodyTf.translation;
            float distanceSq = glm::dot(direction, direction);
            
            if (distanceSq > 0.1f) { 
                float forceMag = (grav.strength * phys.mass) / distanceSq;
                totalForce += glm::normalize(direction) * forceMag;
            }
        }

        if (glm::length(totalForce) > 0.0f) {
            engine.physics().addForce(bodyEntity, totalForce);
        }
    }

    // 3. Spaceship Movement
    state.shipCtrl->update(ship, dt, &engine);

    // 4. Update Cameras
    if (shipCamera.get<CameraComponent>().active) {
        auto target = state.smartCamera->getTarget();
        float altitude = 0.0f;
        glm::vec3 gravityDir = {0.0f, -1.0f, 0.0f};

        if (target == ship) {
            auto& tf = ship.get<TransformComponent>();
            glm::vec3 planetCenter = planet.get<TransformComponent>().translation;
            float dist = glm::length(tf.translation - planetCenter);
            altitude = std::max(0.0f, dist - 10.0f);
            gravityDir = glm::normalize(planetCenter - tf.translation);
        } else if (target == moonEntity) {
            altitude = 5.0f; 
            auto& tf = moonEntity.get<TransformComponent>();
            glm::vec3 planetCenter = planet.get<TransformComponent>().translation;
            gravityDir = glm::normalize(planetCenter - tf.translation);
        }

        glm::vec3 offset = (target == ship) ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(0.0f);
        float manualZoomDelta = input.getMouseScroll().y;
        float manualPitchDelta = 0.0f;
        float manualYawDelta = 0.0f;
        
        if (input.isMouseButtonPressed(bb3d::Mouse::Right)) {
            glm::vec2 delta = input.getMouseDelta();
            manualYawDelta = -delta.x;
            manualPitchDelta = -delta.y; 
        }

        state.smartCamera->update(dt, manualZoomDelta, manualPitchDelta, manualYawDelta, altitude, gravityDir, offset);
        
        // 5. Dynamic Visual Scaling for the Spaceship (makes it visible from orbit ONLY IN MODE 2)
        if (ship.has<TransformComponent>() && planet.has<TransformComponent>()) {
            auto& tf = ship.get<TransformComponent>();
            auto& smartCam = shipCamera.get<bb3d::SmartCameraComponent>();
            
            if (smartCam.mode == 2 && shipCamera.get<CameraComponent>().active) {
                glm::vec3 planetCenter = planet.get<TransformComponent>().translation;
                float dist = glm::length(tf.translation - planetCenter);
                float shipAltitude = std::max(0.0f, dist - 10.0f); // 10.0f is planet base radius
                
                float baseScale = 0.15f;
                float maxScale = 1.5f; 
                
                // Smooth exponential growth mapping (1 - e^(-x)). 
                // At altitude 0, x=0 -> scale=baseScale. As altitude increases, it smoothly approaches maxScale.
                // A constant of 30.0f means at alt=30m, growth is ~63% complete.
                float growthFactor = 1.0f - std::exp(-shipAltitude / 30.0f);
                float currentScale = baseScale + growthFactor * (maxScale - baseScale);
                
                tf.scale = {currentScale, currentScale, currentScale};
            } else {
                // Ensure default scale in other modes
                tf.scale = {0.15f, 0.15f, 0.15f};
            }
            
            // Mode 1: Realistic Pur -> Draw Spatial HUD when high up
            if (smartCam.mode == 1 && shipCamera.get<CameraComponent>().active) {
                glm::vec3 planetCenter = planet.get<TransformComponent>().translation;
                float dist = glm::length(tf.translation - planetCenter);
                float shipAltitude = std::max(0.0f, dist - 10.0f);
                
                if (shipAltitude > 20.0f) {
                    auto& bbCam = shipCamera.get<CameraComponent>().camera;
                    if (bbCam) {
                        auto ubo = bbCam->getUniformData();
                        glm::vec4 clipSpace = ubo.viewProj * glm::vec4(tf.translation, 1.0f);
                        if (clipSpace.w > 0.1f) {
                            glm::vec3 ndc = glm::vec3(clipSpace) / clipSpace.w;
                            if (ndc.z >= 0.0f && ndc.z <= 1.0f && ndc.x >= -1.1f && ndc.x <= 1.1f && ndc.y >= -1.1f && ndc.y <= 1.1f) {
                                float winW = static_cast<float>(engine.GetConfig().window.width);
                                float winH = static_cast<float>(engine.GetConfig().window.height);
                                float screenX = (ndc.x + 1.0f) * 0.5f * winW;
                                float screenY = (ndc.y + 1.0f) * 0.5f * winH;
                                
                                auto drawList = ImGui::GetBackgroundDrawList();
                                ImVec2 p1(screenX - 25, screenY - 25);
                                ImVec2 p2(screenX + 25, screenY + 25);
                                drawList->AddRect(p1, p2, IM_COL32(0, 255, 100, 200), 0.0f, 0, 1.5f);
                                
                                char distStr[32];
                                snprintf(distStr, sizeof(distStr), "ALT: %.0f m", shipAltitude);
                                drawList->AddText(ImVec2(screenX + 30, screenY - 10), IM_COL32(0, 255, 100, 255), distStr);
                            }
                        }
                    }
                }
            }
        }
    }
}

void SetupScene(Engine* engine) {
    auto scene = engine->GetActiveScene();
    if (!scene) return;

    scene->clear();
    
    // Disable global gravity for orbital simulation
    engine->physics().setGravity({0.0f, 0.0f, 0.0f});

    // 1. Create Procedural Planet
    auto planet = scene->createEntity("Planet");
    planet.get<TransformComponent>().translation = {0.0f, 0.0f, 0.0f};
    
    auto& planetComp = planet.add<ProceduralPlanetComponent>().get<ProceduralPlanetComponent>();
    planetComp.radius = 10.0f;
    planetComp.resolution = 128; 
    planetComp.seed = 4242;
    planetComp.globalFrequency = 2.0f;
    planetComp.seaLevel = 10.1f;
    
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
    BB_CORE_INFO("AstroBazard: Planet created with {} meshes", planetComp.model->getMeshes().size());

    auto& planetPhys = planet.add<PhysicsComponent>().get<PhysicsComponent>();
    planetPhys.type = BodyType::Static;
    planetPhys.colliderType = ColliderType::Mesh; 
    planetPhys.useModelMesh = true;
    engine->physics().createRigidBody(planet);
    planet.add<astrobazard::CommRelayComponent>("Prime Colony");
    planet.add<PointGravitySourceComponent>(1000.0f);

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
    // Orbiting Moon
    moonEntity.get<TransformComponent>().translation = { 25.0f, 0.0f, 0.0f };
    moonEntity.add<bb3d::PointGravitySourceComponent>(50.0f);
    moonEntity.add<astrobazard::CommRelayComponent>("Moon Station");
    moonEntity.add<NativeScriptComponent>("MoonOrbit");
    moonEntity.get<NativeScriptComponent>().onUpdate = ComponentRegistry::GetScript("MoonOrbit");

    // 1c. Create Asteroids
    auto asteroidMat = CreateRef<PBRMaterial>(engine->graphics());
    asteroidMat->setColor({0.4f, 0.35f, 0.3f});
    for (int i = 0; i < 20; i++) {
        auto asteroid = scene->createEntity("Asteroid_" + std::to_string(i));
        float orbitalRadius = 18.0f + (rand() % 100) / 10.0f;
        float angle = (rand() % 360) * glm::pi<float>() / 180.0f;
        asteroid.get<TransformComponent>().translation = { std::cos(angle) * orbitalRadius, 0.0f, std::sin(angle) * orbitalRadius };
        float astScale = 0.3f + (rand() % 10) / 20.0f;
        asteroid.get<TransformComponent>().scale = glm::vec3(astScale);

        auto& phys = asteroid.add<PhysicsComponent>().get<PhysicsComponent>();
        phys.type = BodyType::Dynamic;
        phys.colliderType = ColliderType::Sphere;
        phys.radius = 0.5f;
        phys.mass = 2.0f;
        phys.linearDamping = 0.02f;
        phys.angularDamping = 0.1f;
        float orbitalSpeed = std::sqrt(1000.0f / orbitalRadius);
        phys.initialLinearVelocity = glm::vec3(std::cos(angle + glm::half_pi<float>()), 0.0f, std::sin(angle + glm::half_pi<float>())) * orbitalSpeed;
        engine->physics().createRigidBody(asteroid);

        auto& astPlanet = asteroid.add<ProceduralPlanetComponent>().get<ProceduralPlanetComponent>();
        astPlanet.radius = 0.5f; astPlanet.resolution = 8; astPlanet.seed = 42 + i;
        BiomeSettings astBiome; astBiome.name = "Rock"; astBiome.color = {0.4f, 0.35f, 0.3f}; astBiome.heightStart = 0.0f; astBiome.heightEnd = 1.0f; astBiome.amplitude = 0.15f;
        astPlanet.biomes.push_back(astBiome);
        astPlanet.model = ProceduralMeshGenerator::createPlanet(engine->graphics(), engine->assets(), engine->jobs(), astPlanet);
        for (auto& mesh : astPlanet.model->getMeshes()) mesh->setMaterial(asteroidMat);
        asteroid.add<ModelComponent>(astPlanet.model);
    }

    // Environment
    scene->createSkySphere("Cosmos", "assets/textures/skybox_sphere/ringed_gas_giant_planet.png");
    scene->createDirectionalLight("Sun", {1.0f, 0.95f, 0.8f}, 5.0f, {0.8f, -0.6f, 0.0f});

    // Spaceship
    auto shipModel = engine->assets().load<Model>("assets/models/rockets/rocket_2/scene.gltf");
    shipModel->normalize(glm::vec3(1.0f)); 

    auto ship = scene->createEntity("Spaceship");
    ship.get<TransformComponent>().translation = {0.0f, 15.0f, 0.0f};
    ship.add<astrobazard::CommRelayComponent>("Vaisseau Mère");
    
    float baseScale = 0.15f;
    ship.get<TransformComponent>().scale = {baseScale, baseScale, baseScale};
    
    bb3d::AABB bounds = shipModel->getBounds();
    float colliderBottomY = -0.5f; 
    float visualBottomY = bounds.min.y;
    glm::vec3 visualOffset = glm::vec3(0.0f, colliderBottomY - visualBottomY, 0.0f);
    
    auto& modelComp = ship.add<ModelComponent>().get<ModelComponent>();
    modelComp.model = shipModel;
    modelComp.offset = visualOffset;

    auto& phys = ship.add<PhysicsComponent>().get<PhysicsComponent>();
    phys.type = BodyType::Dynamic;
    phys.mass = 1.0f;
    phys.colliderType = ColliderType::Box;
    phys.boxHalfExtents = {0.3f, 0.5f, 0.3f};
    phys.constrain2D = true;
    phys.linearDamping = 0.3f;
    phys.angularDamping = 8.0f;
    phys.initialLinearVelocity = {0.0f, 0.0f, 0.0f};
    engine->physics().createRigidBody(ship);

    auto& ctrl = ship.add<SpaceshipControllerComponent>().get<SpaceshipControllerComponent>();
    ctrl.mainThrustPower = 11.0f;
    ctrl.retroThrustPower = 2.0f;
    ctrl.torquePower = 0.05f;

    auto& ps = ship.add<ParticleSystemComponent>().get<ParticleSystemComponent>();
    auto plasmaTex = engine->assets().load<Texture>("assets/textures/particles/PNG (Black background)/flame_02.png", true);
    auto particleMat = CreateRef<PlasmaMaterial>(engine->graphics());
    if (plasmaTex) particleMat->setBaseMap(plasmaTex);
    particleMat->setIntensity(2.5f);
    ps.material = particleMat;

    // Cameras
    auto shipCamera = scene->createEntity("ShipCamera");
    auto& camComp = shipCamera.add<CameraComponent>().get<CameraComponent>();
    camComp.fov = 60.0f;
    camComp.nearPlane = 1.0f;
    camComp.farPlane = 4000.0f;
    camComp.active = true;
    camComp.camera = CreateRef<Camera>(60.0f, 1280.0f/720.0f, 1.0f, 4000.0f);
    shipCamera.get<TransformComponent>().translation = {0.0f, 2.0f, 12.0f};

    auto& smartCamComp = shipCamera.add<bb3d::SmartCameraComponent>().get<bb3d::SmartCameraComponent>();
    smartCamComp.minZoom = -15.0f;
    smartCamComp.maxZoom = 400.0f;

    auto planetOrbitView = scene->createOrbitCamera("PlanetOrbitCamera", 60.0f, 1280.0f/720.0f, {0,0,0}, 50.0f);
    planetOrbitView.get<CameraComponent>().active = false;

    // Global Logic Script
    auto logicEntity = scene->createEntity("KerbalLogic");
    auto& stateComp = logicEntity.add<AstroBazardStateComponent>().get<AstroBazardStateComponent>();
    stateComp.shipCtrl = std::make_shared<astrobazard::SpaceshipController>();
    stateComp.smartCamera = std::make_shared<astrobazard::SmartCamera>(shipCamera, ship, 20.0f);
    
    auto& logic = logicEntity.add<NativeScriptComponent>("AstroBazardLogic").get<NativeScriptComponent>();
    logic.onUpdate = ComponentRegistry::GetScript("AstroBazardLogic");
}

int main(int argc, char** argv) {
    try {
        // Early logging will be safe once Engine is created


        bool enableEditor = false;
#if defined(BB3D_DEBUG)
        enableEditor = true;
#endif

        auto config = Config::Load("config/engine_config.json")
            .title("AstroBazard - Kerbal Space 2D")
            .resolution(1280, 720)
            .vsync(true)
            .enablePhysics(PhysicsBackend::Jolt)
            .enableOffscreenRendering(true)
            .enableEditor(enableEditor);
            
        config.graphics.setShadows(true, 4096, 4, true);
        auto engine = Engine::Create(config);

        // Logic registration
        bb3d::ComponentRegistry::Register<SimpleTimeComponent>("SimpleTimeComponent");
        bb3d::ComponentRegistry::Register<AstroBazardStateComponent>("AstroBazardStateComponent");
        bb3d::ComponentRegistry::Register<bb3d::SpaceshipControllerComponent>("SpaceshipControllerComponent");
        bb3d::ComponentRegistry::Register<bb3d::SmartCameraComponent>("SmartCameraComponent");
        bb3d::ComponentRegistry::Register<astrobazard::CommRelayComponent>("CommRelayComponent");

        ComponentRegistry::RegisterScript("MoonOrbit", [](Entity entity, float dt) {
            if (!entity.has<SimpleTimeComponent>()) entity.add<SimpleTimeComponent>();
            auto& st = entity.get<SimpleTimeComponent>();
            st.time += dt;
            
            float orbitRadius = 25.0f;
            float orbitSpeed = 0.5f;
            auto& tc = entity.get<TransformComponent>();
            tc.translation = {
                std::cos(st.time * orbitSpeed) * orbitRadius,
                std::sin(st.time * orbitSpeed * 0.5f) * 5.0f, 
                std::sin(st.time * orbitSpeed) * orbitRadius
            };
        });

        ComponentRegistry::RegisterScript("AstroBazardLogic", AstroBazardLogic);

        // Register "Hard Reset" callback
        engine->setOnResetCallback([enginePtr = engine.get()]() {
            SetupScene(enginePtr);
        });

        {
            auto scene = engine->CreateScene();
            engine->SetActiveScene(scene);
            
            SetupScene(engine.get());

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
