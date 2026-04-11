#include "bb3d/core/Log.hpp"
#include "bb3d/scene/Scene.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/scene/SceneSerializer.hpp"
#include "bb3d/scene/ComponentRegistry.hpp"
#include "bb3d/core/Engine.hpp"
#include <iostream>
#include <filesystem>

struct SerialTestComponent {
    int val = 42;
    void serialize(nlohmann::json& j) const { j["val"] = val; }
    void deserialize(const nlohmann::json& j) { if(j.contains("val")) val = j["val"]; }
};

int main() {
    bb3d::EngineConfig logConfig;
    logConfig.system.logDirectory = "unit_test_logs";
    logConfig.system.logFileName = "unit_test_21.log";
    bb3d::Log::Init(logConfig);
    BB_CORE_INFO("--- Test Unitaire 21 : Serialization complex (PRO) ---");

    // Register
    bb3d::ComponentRegistry::Register<SerialTestComponent>("SerialTestComponent");

    // Start Engine
    auto engine = bb3d::Engine::Create(bb3d::EngineConfig().enableEditor(false).enablePhysics(bb3d::PhysicsBackend::None));
    auto scene = engine->CreateScene();
    engine->SetActiveScene(scene);

    // 1. Scene Construction
    {
        auto ent1 = scene->createEntity("Ent1");
        ent1.add<SerialTestComponent>().get<SerialTestComponent>().val = 1337;
        ent1.get<bb3d::TransformComponent>().translation = { 10.0f, 20.0f, 30.0f };

        auto planet = scene->createEntity("PlanetX");
        planet.add<bb3d::ProceduralPlanetComponent>().get<bb3d::ProceduralPlanetComponent>().radius = 1234.0f;
        planet.add<bb3d::PointGravitySourceComponent>(500.0f);
        
        // Test case for ModelComponent with manual model (no assetPath set in component)
        auto ship = scene->createEntity("Ship");
        auto& mc = ship.add<bb3d::ModelComponent>().get<bb3d::ModelComponent>();
        // mc.model = bb3d::CreateRef<bb3d::Model>(engine->graphics(), engine->assets(), "assets/models/house.obj");
        mc.assetPath = "assets/models/house.obj"; // Manually set path for testing serialization
    }
    
    // 2. Serialize
    std::string path = (std::filesystem::current_path() / "test_pro.json").string();
    BB_CORE_INFO("Serializing to: {}", path);
    bb3d::SceneSerializer serializer(scene);
    serializer.serialize(path);
    
    // Check file existence
    if (!std::filesystem::exists(path)) {
        BB_CORE_ERROR("FAILED: Serialized file does not exist at {}!", path);
        return -1;
    }

    // 3. Clear
    try {
        BB_CORE_INFO("Clearing scene...");
        scene->clear();
        BB_CORE_INFO("Scene cleared.");
    } catch (const std::exception& e) {
        BB_CORE_ERROR("Exception during scene clear: {}", e.what());
        return -1;
    } catch (...) {
        BB_CORE_ERROR("Unknown exception during scene clear!");
        return -1;
    }
    
    // 4. Deserialize
    BB_CORE_INFO("Starting Deserialization from: {}", path);
    if (serializer.deserialize(path)) {
        BB_CORE_INFO("Deserialization successful!");
    } else {
        BB_CORE_ERROR("FAILED: Deserialization returned false!");
        return -1;
    }
    BB_CORE_INFO("Checking registry size after deserialize: {}", scene->getRegistry().storage<entt::entity>().size());

    // 5. Validation
    auto view = scene->getRegistry().view<bb3d::TagComponent>();
    bool foundPlanet = false;
    bool foundEnt1 = false;
    
    for (auto e : view) {
        bb3d::Entity entity(e, *scene);
        std::string tag = entity.get<bb3d::TagComponent>().tag;
        BB_CORE_INFO("Validating entity: {}", tag);
        
        if (tag == "PlanetX") {
            foundPlanet = true;
            if (entity.get<bb3d::ProceduralPlanetComponent>().radius != 1234.0f) {
                BB_CORE_ERROR("Radius mismatch: Got {}, expected 1234", entity.get<bb3d::ProceduralPlanetComponent>().radius);
                return -1;
            }
        } else if (tag == "Ent1") {
            foundEnt1 = true;
            if (entity.get<SerialTestComponent>().val != 1337) {
                BB_CORE_ERROR("SerialTestComponent val mismatch!");
                return -1;
            }
        }
    }

    auto shipView = scene->getRegistry().view<bb3d::TagComponent, bb3d::ModelComponent>();
    bool foundShip = false;
    for (auto entity : shipView) {
        if (shipView.get<bb3d::TagComponent>(entity).tag == "Ship") {
            foundShip = true;
            auto& mc = shipView.get<bb3d::ModelComponent>(entity);
            if (mc.assetPath == "assets/models/house.obj") {
                BB_CORE_INFO("PASSED: Ship ModelComponent assetPath correctly retrieved from Resource ('assets/models/house.obj')!");
            } else {
                BB_CORE_ERROR("FAILED: Ship ModelComponent assetPath is '{}', expected 'assets/models/house.obj'!", mc.assetPath);
                return -1;
            }
        }
    }

    if (!foundPlanet || !foundEnt1 || !foundShip) {
        BB_CORE_ERROR("Failed to find back all entities!");
        return -1;
    }

    BB_CORE_INFO("--- Test 21 (PRO) PASSED ---");

    std::filesystem::remove(path);
    engine->Shutdown();
    return 0;
}
