#include "bb3d/render/ProceduralMeshGenerator.hpp"
#include <glm/gtc/noise.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <memory>
#include <atomic>
#include "bb3d/core/JobSystem.hpp"

namespace bb3d {

static float getElevationAt(const glm::vec3& pointOnSphere, const ProceduralPlanetComponent& component) {
    // 1. Humidity
    float humidity = glm::perlin(pointOnSphere * component.globalFrequency + (float)component.seed);
    humidity = (humidity + 1.0f) * 0.5f;

    if (component.biomes.empty()) {
        return glm::perlin(pointOnSphere * 2.0f) * component.globalAmplitude;
    }

    float totalElevation = 0.0f;
    float totalWeight = 0.0f;

    for (const auto& biome : component.biomes) {
        // Smooth transition between biomes
        float range = biome.heightEnd - biome.heightStart;
        float center = (biome.heightStart + biome.heightEnd) * 0.5f;
        float dist = std::abs(humidity - center);
        float weight = std::max(0.0f, 1.0f - (dist / (range * 0.5f)));
        
        if (weight > 0.0f) {
            // Apply smoothstep to weight
            weight = weight * weight * (3.0f - 2.0f * weight);
            
            float biomeNoise = 0.0f;
            float freq = biome.frequency;
            float amp = biome.amplitude;
            for (uint32_t i = 0; i < biome.octaves; i++) {
                biomeNoise += glm::perlin(pointOnSphere * freq) * amp;
                freq *= biome.lacunarity;
                amp *= biome.persistence;
            }
            totalElevation += biomeNoise * weight;
            totalWeight += weight;
        }
    }

    return totalWeight > 0.0f ? (totalElevation / totalWeight) : 0.0f;
}

static glm::vec3 getBiomeColorAt(const glm::vec3& pointOnSphere, const ProceduralPlanetComponent& component) {
    float humidity = glm::perlin(pointOnSphere * component.globalFrequency + (float)component.seed);
    humidity = (humidity + 1.0f) * 0.5f;

    if (component.biomes.empty()) return glm::vec3(0.8f);

    glm::vec3 blendedColor(0.0f);
    float totalWeight = 0.0f;

    for (const auto& biome : component.biomes) {
        float range = biome.heightEnd - biome.heightStart;
        float center = (biome.heightStart + biome.heightEnd) * 0.5f;
        float dist = std::abs(humidity - center);
        float weight = std::max(0.0f, 1.0f - (dist / (range * 0.5f)));
        
        if (weight > 0.0f) {
            weight = weight * weight * (3.0f - 2.0f * weight);
            blendedColor += biome.color * weight;
            totalWeight += weight;
        }
    }
    return totalWeight > 0.0f ? (blendedColor / totalWeight) : glm::vec3(0.8f);
}

Scope<Model> ProceduralMeshGenerator::createPlanet(VulkanContext& context, ResourceManager& resourceManager, JobSystem& jobSystem, const ProceduralPlanetComponent& component) {
    auto model = CreateScope<Model>(context, resourceManager);

    std::vector<glm::vec3> faceDirections = {
        { 1.0f,  0.0f,  0.0f}, {-1.0f,  0.0f,  0.0f},
        { 0.0f,  1.0f,  0.0f}, { 0.0f, -1.0f,  0.0f},
        { 0.0f,  0.0f,  1.0f}, { 0.0f,  0.0f, -1.0f}
    };

    JobCounter counter = std::make_shared<std::atomic<int>>(6);
    std::vector<FaceData> faceData(6);

    // 1. Parallel Generation of CPU data (Vertices/Indices)
    for (int i = 0; i < 6; i++) {
        jobSystem.execute([&, i]() {
            faceData[i] = generateFaceData(faceDirections[i], component);
        }, counter);
    }
    jobSystem.wait(counter);

    // 2. Sequential Upload to GPU (Must be on main thread or synchronized)
    for (int i = 0; i < 6; i++) {
        auto mesh = CreateRef<Mesh>(context, faceData[i].vertices, faceData[i].indices);
        model->addMesh(std::move(mesh));
    }

    return model;
}

ProceduralMeshGenerator::FaceData ProceduralMeshGenerator::generateFaceData(
    const glm::vec3& localUp, 
    const ProceduralPlanetComponent& component
) {
    uint32_t resolution = component.resolution;
    uint32_t vCount = (resolution + 1) * (resolution + 1);
    uint32_t iCount = resolution * resolution * 6;
    
    FaceData data;
    data.vertices.resize(vCount);
    data.indices.resize(iCount);

    glm::vec3 axisA(localUp.y, localUp.z, localUp.x);
    glm::vec3 axisB = glm::cross(localUp, axisA);

    // --- Optimization: Cache elevations in a grid ---
    // This avoids sampling noise 3x per vertex for normal estimation.
    std::vector<float> elevations(vCount);
    
    auto getPointOnSphere = [&](float px, float py) {
        glm::vec3 p = localUp + (px - 0.5f) * 2.0f * axisA + (py - 0.5f) * 2.0f * axisB;
        return glm::normalize(p);
    };

    // Phase 1: Sample all elevations
    for (uint32_t y = 0; y <= resolution; y++) {
        for (uint32_t x = 0; x <= resolution; x++) {
            glm::vec2 percent = glm::vec2(x, y) / (float)resolution;
            glm::vec3 p = getPointOnSphere(percent.x, percent.y);
            elevations[x + y * (resolution + 1)] = getElevationAt(p, component);
        }
    }

    // Phase 2: Compute vertices (Positions and Normals)
    for (uint32_t y = 0; y <= resolution; y++) {
        for (uint32_t x = 0; x <= resolution; x++) {
            uint32_t idx = x + y * (resolution + 1);
            glm::vec2 percent = glm::vec2(x, y) / (float)resolution;
            glm::vec3 p0 = getPointOnSphere(percent.x, percent.y);
            
            float h0 = elevations[idx];
            float finalRadius0 = std::max(component.radius + h0, component.seaLevel);
            glm::vec3 pos0 = p0 * finalRadius0;

            // Normal estimation using central differences from our cached grid
            // We use neighbor vertices to estimate the surface tangent
            glm::vec3 normal;
            if (x < resolution && y < resolution) {
                // Get neighbors in the grid
                float hRight = elevations[(x + 1) + y * (resolution + 1)];
                float hDown = elevations[x + (y + 1) * (resolution + 1)];
                
                glm::vec3 pRight = getPointOnSphere(percent.x + 1.0f/resolution, percent.y);
                glm::vec3 pDown = getPointOnSphere(percent.x, percent.y + 1.0f/resolution);
                
                glm::vec3 posRight = pRight * std::max(component.radius + hRight, component.seaLevel);
                glm::vec3 posDown = pDown * std::max(component.radius + hDown, component.seaLevel);
                
                normal = glm::normalize(glm::cross(posRight - pos0, posDown - pos0));
                // Ensure it faces outward (normalization might flip it depending on cross order)
                if (glm::dot(normal, p0) < 0) normal = -normal;
            } else if (x > 0 && y > 0) {
                // Use reverse neighbors for the edges
                float hLeft = elevations[(x - 1) + y * (resolution + 1)];
                float hUp = elevations[x + (y - 1) * (resolution + 1)];
                
                glm::vec3 pLeft = getPointOnSphere(percent.x - 1.0f/resolution, percent.y);
                glm::vec3 pUp = getPointOnSphere(percent.x, percent.y - 1.0f/resolution);
                
                glm::vec3 posLeft = pLeft * std::max(component.radius + hLeft, component.seaLevel);
                glm::vec3 posUp = pUp * std::max(component.radius + hUp, component.seaLevel);
                
                normal = glm::normalize(glm::cross(posUp - pos0, posLeft - pos0));
                if (glm::dot(normal, p0) < 0) normal = -normal;
            } else {
                // Fallback for tricky corners: use sphere normal
                normal = p0;
            }

            // Ocean color logic
            glm::vec3 color = getBiomeColorAt(p0, component);
            if (finalRadius0 <= component.seaLevel + 1e-5f) {
                color = glm::vec3(0.05f, 0.15f, 0.5f);
                normal = p0; // Sea is flat/sphere-aligned
            }

            data.vertices[idx] = {
                pos0, normal, color, percent, glm::vec4(axisA, 1.0f)
            };
        }
    }

    uint32_t idx = 0;
    for (uint32_t y = 0; y < resolution; y++) {
        for (uint32_t x = 0; x < resolution; x++) {
            uint32_t i = x + y * (resolution + 1);
            data.indices[idx++] = i;
            data.indices[idx++] = i + resolution + 2;
            data.indices[idx++] = i + resolution + 1;
            data.indices[idx++] = i;
            data.indices[idx++] = i + 1;
            data.indices[idx++] = i + resolution + 2;
        }
    }

    return data;
}

} // namespace bb3d
