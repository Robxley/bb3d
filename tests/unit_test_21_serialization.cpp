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
    scene->clear();
    BB_CORE_INFO("Scene cleared. Registry size: {}", scene->getRegistry().size());
    
    // 4. Deserialize
    if (serializer.deserialize(path)) {
        BB_CORE_INFO("Deserialization successful!");
    } else {
        BB_CORE_ERROR("FAILED: Deserialization returned false!");
        return -1;
    }

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

    if (!foundPlanet || !foundEnt1) {
        BB_CORE_ERROR("Failed to find back all entities!");
        return -1;
    }

    BB_CORE_INFO("--- Test 21 (PRO) PASSED ---");

    std::filesystem::remove(path);
    engine->Shutdown();
    return 0;
}
