#include "bb3d/core/Log.hpp"
#include "bb3d/scene/Scene.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/physics/PhysicsWorld.hpp"
#include <cassert>
#include <iostream>

using namespace bb3d;

int main() {
    EngineConfig logConfig;
    logConfig.system.logDirectory = "unit_test_logs";
    logConfig.system.logFileName = "unit_test_27.log";
    Log::Init(logConfig);
    BB_CORE_INFO("=== Test Unitaire 27 : Robustesse Physique Jolt (B14, B15, B16, B20, B21) ===");

    // --- Test 1: B14 - PhysicsWorld::init() avec thread count sécurisé ---
    BB_CORE_INFO("Test 1 (B14): Initialisation de PhysicsWorld...");
    PhysicsWorld physics;
    physics.init();
    BB_CORE_INFO("Test 1 PASSED: PhysicsWorld initialisé avec succès.");

    // --- Test 2: B15 - createRigidBody sur entité sans TransformComponent ---
    BB_CORE_INFO("Test 2 (B15): Appel createRigidBody sans TransformComponent...");
    Scene scene;
    // Entité créée manuellement dans le registre EnTT sans TransformComponent
    entt::entity rawHandle = scene.getRegistry().create();
    Entity noTransformEntity(rawHandle, scene);
    noTransformEntity.add<PhysicsComponent>();
    auto& physComp = noTransformEntity.get<PhysicsComponent>();
    physComp.type = BodyType::Dynamic;
    physComp.colliderType = ColliderType::Box;
    physComp.boxHalfExtents = glm::vec3(1.0f);

    // Ne doit ni lancer d'exception ni crasher
    physics.createRigidBody(noTransformEntity);
    assert(physComp.bodyID == 0xFFFFFFFF && "bodyID should remain 0xFFFFFFFF when Transform is missing");
    BB_CORE_INFO("Test 2 PASSED: createRigidBody a ignoré gracieusement l'entité sans TransformComponent.");

    // --- Test 3: B20 - createCharacterController sur entité sans TransformComponent ---
    BB_CORE_INFO("Test 3 (B20): Appel createCharacterController sans TransformComponent...");
    entt::entity rawHandleCC = scene.getRegistry().create();
    Entity noTransformCCEntity(rawHandleCC, scene);
    noTransformCCEntity.add<CharacterControllerComponent>();

    // Ne doit ni lancer d'exception ni crasher
    physics.createCharacterController(noTransformCCEntity);
    BB_CORE_INFO("Test 3 PASSED: createCharacterController a ignoré gracieusement l'entité sans TransformComponent.");

    // --- Test 4: B16 - createRigidBody nominal sur entité avec TransformComponent ---
    BB_CORE_INFO("Test 4 (B16): Création nominale de RigidBody avec TransformComponent...");
    auto validEntity = scene.createEntity("ValidPhysicsObject");
    validEntity.at({0.0f, 10.0f, 0.0f});
    auto& validPhys = validEntity.add<PhysicsComponent>().get<PhysicsComponent>();
    validPhys.type = BodyType::Dynamic;
    validPhys.colliderType = ColliderType::Box;
    validPhys.boxHalfExtents = glm::vec3(0.5f);

    physics.createRigidBody(validEntity);
    assert(validPhys.bodyID != 0xFFFFFFFF && "RigidBody should have a valid bodyID");
    BB_CORE_INFO("Test 4 PASSED: RigidBody créé avec ID: {}", validPhys.bodyID);

    // --- Test 5: B21 - destroyRigidBody libère bien le corps ---
    BB_CORE_INFO("Test 5 (B21): Destruction explicite du corps physique...");
    uint32_t oldBodyID = validPhys.bodyID;
    physics.destroyRigidBody(validEntity);
    assert(validPhys.bodyID == 0xFFFFFFFF && "bodyID should be reset to 0xFFFFFFFF after destroyRigidBody");
    BB_CORE_INFO("Test 5 PASSED: RigidBody {} détruit et réinitialisé à 0xFFFFFFFF.", oldBodyID);

    // --- Test 6: Nettoyage et shutdown ---
    BB_CORE_INFO("Test 6: Simulation step & PhysicsWorld::shutdown()...");
    physics.update(1.0f / 60.0f, scene);
    physics.shutdown();
    BB_CORE_INFO("Test 6 PASSED: Shutdown propre effectué.");

    BB_CORE_INFO("=== TOUS LES TESTS DE ROBUSTESSE PHYSIQUE ONT RÉUSSI ===");
    return 0;
}
