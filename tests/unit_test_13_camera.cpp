#include "bb3d/core/Log.hpp"
#include "bb3d/core/Config.hpp"
#include "bb3d/scene/FPSCamera.hpp"
#include "bb3d/scene/OrbitCamera.hpp"
#include <iostream>
#include <glm/gtc/epsilon.hpp>

#define BB_TEST_CASE(name) void name()
#define BB_ASSERT_TRUE(cond, msg) if (!(cond)) { BB_CORE_ERROR("Test Fail: {}", msg); return; }

BB_TEST_CASE(TestFPSCamera) {
    BB_CORE_INFO("Test: FPSCamera Logic...");
    bb3d::FPSCamera cam(45.0f, 1.0f, 0.1f, 100.0f);
    
    cam.setPosition({0, 0, 0});
    cam.setRotation(-90.0f, 0.0f); // Regarde vers -Z
    cam.update(0.0f);

    auto view = cam.getViewMatrix();
    // Le point (0,0,-5) doit être au centre de l'écran après projection
    glm::vec4 point(0, 0, -5, 1);
    glm::vec4 transformed = view * point;
    BB_ASSERT_TRUE(glm::epsilonEqual(transformed.z, -5.0f, 0.001f), "FPSCamera view matrix Z failed");

    cam.move({0, 0, 1}, 1.0f); // Avance de speed * 1.0
    BB_ASSERT_TRUE(cam.getPosition().z < 0.0f, "FPSCamera move forward failed");
    
    BB_CORE_INFO("[Success] FPSCamera logic passed.");
}

BB_TEST_CASE(TestOrbitCamera) {
    BB_CORE_INFO("Test: OrbitCamera Logic...");
    bb3d::OrbitCamera cam(45.0f, 1.0f, 0.1f, 100.0f);
    
    cam.setTarget({0, 0, 0});
    // Pas de rotation -> Doit être sur Z+ (yaw=0, pitch=0)
    cam.update(0.0f);

    BB_ASSERT_TRUE(glm::epsilonEqual(cam.getPosition().z, 5.0f, 0.001f), "OrbitCamera default distance failed");

    // Tourner de 90 deg (en tenant compte de la sensibilité 0.1 par défaut)
    // Pour avoir 90 deg effectifs, on doit passer 900.0f
    cam.rotate(900.0f, 0.0f); 
    cam.update(0.0f);
    
    // Yaw = 90 -> Doit être sur X+
    BB_ASSERT_TRUE(glm::epsilonEqual(cam.getPosition().x, 5.0f, 0.001f), "OrbitCamera rotation failed");

    BB_CORE_INFO("[Success] OrbitCamera logic passed.");
}

int main() {
    bb3d::EngineConfig logConfig;
    logConfig.system.logDirectory = "unit_test_logs";
    bb3d::Log::Init(logConfig);
    BB_CORE_INFO("Test Unitaire 13 : Caméras (Logique)");

    TestFPSCamera();
    TestOrbitCamera();

    return 0;
}
