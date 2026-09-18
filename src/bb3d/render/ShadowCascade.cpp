#include "bb3d/render/ShadowCascade.hpp"
#include <algorithm>
#include <array>
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
    // 1. Unproject the 4 corner rays of the camera frustum from NDC to view space.
    // Vulkan NDC: x in [-1, 1], y in [-1, 1], z in [0, 1].
    glm::mat4 invProj = glm::inverse(cameraProj);
    glm::mat4 invView = glm::inverse(cameraView);

    const glm::vec2 ndcCorners[4] = {
        {-1.0f, -1.0f}, // Top-Left
        { 1.0f, -1.0f}, // Top-Right
        {-1.0f,  1.0f}, // Bottom-Left
        { 1.0f,  1.0f}  // Bottom-Right
    };

    std::array<glm::vec3, 8> worldCorners{};
    glm::vec3 center(0.0f);

    for (int i = 0; i < 4; ++i) {
        // Unproject point on the far plane (z = 1.0f in Vulkan NDC)
        glm::vec4 pFar = invProj * glm::vec4(ndcCorners[i].x, ndcCorners[i].y, 1.0f, 1.0f);
        glm::vec3 vFar = glm::vec3(pFar) / pFar.w;

        // In standard camera view space (looking along -Z), depth is -z.
        // Normalize the ray by its depth so that ray * (-distance) gives the view-space point.
        float depth = std::abs(vFar.z);
        glm::vec3 rayDir = (depth > 1e-6f) ? (vFar / depth) : glm::vec3(0.0f, 0.0f, -1.0f);

        // Compute sub-frustum near and far corners in view space.
        // rayDir has rayDir.z == -1.0f (looking down -Z), so multiplying by nearZ/farZ puts them in front of camera.
        glm::vec3 vNearCorner = rayDir * nearZ;
        glm::vec3 vFarCorner = rayDir * farZ;

        // Transform to world space
        glm::vec3 wNear = glm::vec3(invView * glm::vec4(vNearCorner, 1.0f));
        glm::vec3 wFar = glm::vec3(invView * glm::vec4(vFarCorner, 1.0f));

        worldCorners[i] = wNear;
        worldCorners[i + 4] = wFar;

        center += wNear + wFar;
    }
    center /= 8.0f;

    // 2. Compute isotropic bounding sphere around the sub-frustum
    float radius = 0.0f;
    for (const auto& corner : worldCorners) {
        radius = std::max(radius, glm::distance(corner, center));
    }
    // Stabilize radius to prevent precision jitter
    radius = std::ceil(radius * 16.0f) / 16.0f;

    // 3. Position the light camera along lightDir pointing at center.
    // Ensure sufficient back margin to encompass shadow casters located between the light and the frustum.
    glm::vec3 normalizedLightDir = glm::normalize(lightDir);
    float backMargin = radius * 4.0f + 50.0f;
    glm::vec3 eye = center - normalizedLightDir * backMargin;
    glm::vec3 up = (std::abs(normalizedLightDir.y) > 0.99f) ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
    glm::mat4 lightView = glm::lookAt(eye, center, up);

    // 4. Symmetric orthographic projection based on the bounding sphere radius.
    // Slight margin (5%) to guarantee that sub-frustum corners remain inside after texel snapping.
    float extents = radius * 1.05f;
    float zNearOrtho = 0.0f;
    float zFarOrtho = backMargin + radius * 2.0f;

    glm::mat4 lightProj = glm::ortho(-extents, extents, -extents, extents, zNearOrtho, zFarOrtho);

    // Vulkan Y-flip for depth projection
    lightProj[1][1] *= -1.0f;

    // 5. Texel Snapping to prevent shadow edge shimmering when camera moves/rotates
    glm::mat4 shadowMatrix = lightProj * lightView;
    glm::vec4 shadowOrigin = shadowMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    shadowOrigin = shadowOrigin * (static_cast<float>(shadowMapRes) * 0.5f);

    glm::vec4 roundedOrigin = glm::round(shadowOrigin);
    glm::vec4 roundOffset = (roundedOrigin - shadowOrigin) * (2.0f / static_cast<float>(shadowMapRes));
    roundOffset.z = 0.0f;
    roundOffset.w = 0.0f;

    lightProj[3] += roundOffset;

    return lightProj * lightView;
}

} // namespace bb3d
