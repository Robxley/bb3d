#include "bb3d/render/ShadowCascade.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <glm/gtc/matrix_transform.hpp>

namespace bb3d {

std::vector<float> ShadowCascade::calculateSplitDistances(uint32_t cascadeCount, float nearPlane, float farPlane, float lambda) {
    std::vector<float> splits(cascadeCount);
    for (uint32_t i = 1; i <= cascadeCount; i++) {
        float p = static_cast<float>(i) / static_cast<float>(cascadeCount);
        float log = nearPlane * std::pow(farPlane / nearPlane, p);
        float uniform = nearPlane + (farPlane - nearPlane) * p;
        splits[i - 1] = log * lambda + uniform * (1.0f - lambda);
    }
    return splits;
}

glm::mat4 ShadowCascade::calculateLightSpaceMatrix(
    const glm::mat4& cameraProj, 
    const glm::mat4& cameraView, 
    const glm::vec3& lightDir, 
    float nearZ, 
    float farZ, 
    uint32_t shadowMapRes
) {
    // 1. Get the sub-frustum corners in view space.
    glm::mat4 invProj = glm::inverse(cameraProj);
    std::vector<glm::vec4> viewCorners;
    for (unsigned int x = 0; x < 2; ++x) {
        for (unsigned int y = 0; y < 2; ++y) {
            for (unsigned int z = 0; z < 2; ++z) {
                glm::vec4 pt = invProj * glm::vec4(2.0f * x - 1.0f, 2.0f * y - 1.0f, z, 1.0f);
                viewCorners.push_back(pt / pt.w);
            }
        }
    }
    
    // 2. Interpolate to get world space corners based on cascade splits
    glm::mat4 invView = glm::inverse(cameraView);
    std::vector<glm::vec3> worldCorners;
    glm::vec3 center(0.0f);
    
    for (int i = 0; i < 4; ++i) {
        glm::vec3 dir = glm::normalize(glm::vec3(viewCorners[i + 4])); 
        
        glm::vec3 wNear = glm::vec3(invView * glm::vec4(dir * nearZ, 1.0f));
        glm::vec3 wFar = glm::vec3(invView * glm::vec4(dir * farZ, 1.0f));
        
        worldCorners.push_back(wNear);
        worldCorners.push_back(wFar);
        
        center += wNear;
        center += wFar;
    }
    center /= 8.0f;

    // 3. Compute Bounding Sphere Radius around the frustum
    float radius = 0.0f;
    for (const auto& v : worldCorners) {
        radius = std::max(radius, glm::length(v - center));
    }
    radius = std::ceil(radius * 16.0f) / 16.0f; // Stabilize radius

    // 4. Calculate Light View properly positioned behind the bounding sphere
    float backMargin = radius * 3.0f + 1000.0f; 
    glm::vec3 eye = center - lightDir * backMargin;
    glm::vec3 up = std::abs(lightDir.y) > 0.999f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
    glm::mat4 lightView = glm::lookAt(eye, center, up);

    // 5. Fixed Orthographic Matrix based strictly on sphere radius
    float extents = radius; 
    float zNear_ortho = 0.0f; 
    float zFar_ortho = backMargin + radius + 1000.0f;
    
    glm::mat4 lightProj = glm::ortho(-extents, extents, -extents, extents, zNear_ortho, zFar_ortho);
    
    // IMPORTANT: Fix Y axis for Vulkan depth projection mappings
    lightProj[1][1] *= -1.0f;
    
    // 6. Texel Snapping 
    glm::mat4 shadowMatrix = lightProj * lightView;
    glm::vec4 shadowOrigin = shadowMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    shadowOrigin = shadowOrigin * (float)shadowMapRes / 2.0f;
    
    glm::vec4 roundedOrigin = glm::round(shadowOrigin);
    glm::vec4 roundOffset = roundedOrigin - shadowOrigin;
    roundOffset = roundOffset * 2.0f / (float)shadowMapRes;
    roundOffset.z = 0.0f; 
    roundOffset.w = 0.0f;
    
    lightProj[3] += roundOffset;

    return lightProj * lightView;
}

} // namespace bb3d
