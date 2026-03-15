#include "bb3d/core/Config.hpp"
#include "bb3d/core/Engine.hpp"
#include "bb3d/render/ShadowCascade.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <cassert>

using namespace bb3d;

int main(int argc, char* argv[]) {
    std::cout << "--- Unit Test: Shadow Configuration & Cascades ---\n";
    
    EngineConfig cfg;

    // Check default values for CSM Shadows
    assert(cfg.graphics.shadowsEnabled == true);
    assert(cfg.graphics.shadowCascades == 4);
    assert(cfg.graphics.shadowMapResolution == 2048);
    assert(cfg.graphics.shadowPCF == true);
    
    // Test the generation of the Shadow Render Pass
    cfg.window.setResolution(800, 600);
    cfg.modules.enablePhysics = false;
    cfg.modules.enableAudio = false;
    
    auto engine = Engine::Create(cfg);
    assert(engine != nullptr);
    
    auto& renderer = engine->renderer();
    assert(renderer.getShadowDepthImageView() != vk::ImageView(nullptr));
    
    // --- Test 3: Mathematical CPU Cascades ---
    std::cout << "Testing ShadowCascade Math...\n";
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
    uint32_t cascadeCount = 4;
    
    auto splits = ShadowCascade::calculateSplitDistances(cascadeCount, nearPlane, farPlane, 0.5f);
    assert(splits.size() == cascadeCount);
    assert(splits[0] > nearPlane); // La distance du premier split doit être superieur au near plane.
    assert(splits.back() <= farPlane); // La derniere distance doit etre proche du far plane.
    
    glm::mat4 cameraProj = glm::perspective(glm::radians(45.0f), 16.0f/9.0f, nearPlane, farPlane);
    glm::mat4 cameraView = glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));
    glm::vec3 lightDir = glm::normalize(glm::vec3(-1.0f, -1.0f, -1.0f));
    
    glm::mat4 lightVP = ShadowCascade::calculateLightSpaceMatrix(cameraProj, cameraView, lightDir, nearPlane, splits[0], cfg.graphics.shadowMapResolution);
    
    // Une identité indiquerait le stub par defaut
    assert(lightVP != glm::mat4(1.0f)); 

    std::cout << "All shadow configuration, passes, and maths are correctly validated!\n";
    return 0;
}
