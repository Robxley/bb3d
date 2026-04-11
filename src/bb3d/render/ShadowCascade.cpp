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
    // 1. Calculate inverse projection-view to get frustum corners
    // However, we only want the corners for the sub-frustum [nearZ, farZ]
    // To do this reliably, we'll reconstruct a projection matrix with the sub-frustum ranges
    // assuming cameraProj is a standard perspective projection.
    
    // Extract FOV and Aspect from original proj if possible, but easier to just use standard math:
    // We already have cameraProj and cameraView. Let's use the 0..1 NDC for Vulkan.
    
    glm::mat4 invCam = glm::inverse(cameraProj * cameraView);
    
    std::vector<glm::vec4> frustumCorners;
    for (unsigned int x = 0; x < 2; ++x) {
        for (unsigned int y = 0; y < 2; ++y) {
            for (unsigned int z = 0; z < 2; ++z) {
                glm::vec4 pt = invCam * glm::vec4(
                    2.0f * x - 1.0f,
                    2.0f * y - 1.0f,
                    z, // Vulkan NDC z is [0, 1]
                    1.0f);
                frustumCorners.push_back(pt / pt.w);
            }
        }
    }

    // Now refine the 8 corners to match the sub-frustum [nearZ, farZ]
    // Since nearZ/farZ are in view space, we use the view-space corners
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
    
    // The viewCorners z values are likely the original near/far of the camera.
    // We want to interpolate to get the sub-frustum corners in world space.
    glm::vec3 center(0.0f);
    std::vector<glm::vec3> worldCorners;
    glm::mat4 invView = glm::inverse(cameraView);

    // Get the corners of the camera's near and far frustum slices in view space
    // Then interpolate to get the nearZ and farZ slices.
    // Near slice is z=0 (NDC), Far slice is z=1 (NDC)
    // In view space, we can just project the 4 rays.
    for (int i = 0; i < 4; ++i) {
        glm::vec3 dir = glm::normalize(glm::vec3(viewCorners[i + 4])); // direction to far corner
        worldCorners.push_back(glm::vec3(invView * glm::vec4(dir * nearZ, 1.0f)));
        worldCorners.push_back(glm::vec3(invView * glm::vec4(dir * farZ, 1.0f)));
    }

    for (const auto& v : worldCorners) center += v;
    center /= worldCorners.size();

    // Use a large enough distance to avoid clipping casters behind the camera
    float distToCenter = farZ * 1.5f + 100.0f;
    glm::vec3 eye = center - lightDir * distToCenter;

    glm::vec3 up = std::abs(lightDir.y) > 0.99f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
    glm::mat4 lightView = glm::lookAt(eye, center, up);

    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    float minZ = std::numeric_limits<float>::max();
    float maxZ = std::numeric_limits<float>::lowest();

    for (const auto& v : worldCorners) {
        const auto trf = lightView * glm::vec4(v, 1.0f);
        minX = std::min(minX, trf.x);
        maxX = std::max(maxX, trf.x);
        minY = std::min(minY, trf.y);
        maxY = std::max(maxY, trf.y);
        minZ = std::min(minZ, trf.z);
        maxZ = std::max(maxZ, trf.z);
    }

    // In RH lookAt, points ahead are at -Z. 
    // We want near (closest to eye) to be zNear and far (furthest) to be zFar.
    // Vulkan maps [zNear, zFar] to [0, 1].
    // Our points are roughly between Z=-2*farZ and Z=0 (actually they are centered around -distToCenter).
    
    // Near plane is the one with LARGEST Z (closest to 0). Far plane is the SMALLEST Z.
    // Near = closest to eye = -maxZ? No.
    // Simple: Use distance. near = 0 (at eye), far = distToCenter + farZ (frustum far).
    // Or better, tightly fit:
    float zNear = -maxZ - 1000.0f; // Include casters
    float zFar = -minZ;

    // Stability: Round to texel size to avoid shimmering
    // 5. Stabilize projection by snapping to texel increments
    float worldUnitsPerTexel = (maxX - minX) / (float)shadowMapRes;
    minX = std::floor(minX / worldUnitsPerTexel) * worldUnitsPerTexel;
    maxX = std::ceil(maxX / worldUnitsPerTexel) * worldUnitsPerTexel;
    minY = std::floor(minY / worldUnitsPerTexel) * worldUnitsPerTexel;
    maxY = std::ceil(maxY / worldUnitsPerTexel) * worldUnitsPerTexel;

    float zNear_ortho = -500.0f; // Extend towards the light to include distant casters
    float zFar_ortho = distToCenter * 2.1f + 100.0f;
    glm::mat4 lightProj = glm::ortho(minX, maxX, minY, maxY, zNear_ortho, zFar_ortho);
    return lightProj * lightView;
}

} // namespace bb3d
