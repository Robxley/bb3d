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
    cameraProj[1][1] *= -1; // Vulkan Y-flip
    glm::mat4 cameraView = glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));
    glm::vec3 lightDir = glm::normalize(glm::vec3(-1.0f, -1.0f, -1.0f));
    
    glm::mat4 lightVP = ShadowCascade::calculateLightSpaceMatrix(cameraProj, cameraView, lightDir, nearPlane, splits[0], cfg.graphics.shadowMapResolution);
    
    // Une identité indiquerait le stub par defaut
    assert(lightVP != glm::mat4(1.0f)); 

    // --- Test 4: Sub-frustum Corners Coverage (Including Left & Right) ---
    std::cout << "Testing Left and Right sub-frustum coverage in light space...\n";
    float tanHalfFov = std::tan(glm::radians(45.0f) * 0.5f);
    float aspect = 16.0f / 9.0f;
    float cascadeFar = splits[0];
    float halfWidthFar = tanHalfFov * cascadeFar * aspect;
    float halfHeightFar = tanHalfFov * cascadeFar;

    // Test Left Far Corner (x = -1 in NDC, should definitely be covered by the light projection)
    glm::vec3 leftFarPoint(-halfWidthFar, 0.0f, -cascadeFar);
    glm::vec4 leftFarClip = lightVP * glm::vec4(leftFarPoint, 1.0f);
    glm::vec3 leftFarNdc = glm::vec3(leftFarClip) / leftFarClip.w;

    std::cout << "Left Far NDC: x=" << leftFarNdc.x << ", y=" << leftFarNdc.y << ", z=" << leftFarNdc.z << "\n";
    // The left corner must project inside the light frustum [-1, 1] x [-1, 1]
    assert(leftFarNdc.x >= -1.05f && leftFarNdc.x <= 1.05f);
    assert(leftFarNdc.y >= -1.05f && leftFarNdc.y <= 1.05f);
    assert(leftFarNdc.z >= -0.05f && leftFarNdc.z <= 1.05f);

    // Test Right Far Corner (x = +1 in NDC)
    glm::vec3 rightFarPoint(halfWidthFar, 0.0f, -cascadeFar);
    glm::vec4 rightFarClip = lightVP * glm::vec4(rightFarPoint, 1.0f);
    glm::vec3 rightFarNdc = glm::vec3(rightFarClip) / rightFarClip.w;

    std::cout << "Right Far NDC: x=" << rightFarNdc.x << ", y=" << rightFarNdc.y << ", z=" << rightFarNdc.z << "\n";
    assert(rightFarNdc.x >= -1.05f && rightFarNdc.x <= 1.05f);
    assert(rightFarNdc.y >= -1.05f && rightFarNdc.y <= 1.05f);
    assert(rightFarNdc.z >= -0.05f && rightFarNdc.z <= 1.05f);

    // Test Top Far Corner
    glm::vec3 topFarPoint(0.0f, halfHeightFar, -cascadeFar);
    glm::vec4 topFarClip = lightVP * glm::vec4(topFarPoint, 1.0f);
    glm::vec3 topFarNdc = glm::vec3(topFarClip) / topFarClip.w;
    assert(topFarNdc.x >= -1.05f && topFarNdc.x <= 1.05f);
    assert(topFarNdc.y >= -1.05f && topFarNdc.y <= 1.05f);
    assert(topFarNdc.z >= -0.05f && topFarNdc.z <= 1.05f);

    // Test Bottom Far Corner
    glm::vec3 bottomFarPoint(0.0f, -halfHeightFar, -cascadeFar);
    glm::vec4 bottomFarClip = lightVP * glm::vec4(bottomFarPoint, 1.0f);
    glm::vec3 bottomFarNdc = glm::vec3(bottomFarClip) / bottomFarClip.w;
    assert(bottomFarNdc.x >= -1.05f && bottomFarNdc.x <= 1.05f);
    assert(bottomFarNdc.y >= -1.05f && bottomFarNdc.y <= 1.05f);
    assert(bottomFarNdc.z >= -0.05f && bottomFarNdc.z <= 1.05f);

    std::cout << "All shadow configuration, passes, and maths are correctly validated!\n";
    return 0;
}
