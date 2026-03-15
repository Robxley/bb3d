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
    float epsilon = 0.001f;

    for (uint32_t y = 0; y <= resolution; y++) {
        for (uint32_t x = 0; x <= resolution; x++) {
            glm::vec2 percent = glm::vec2(x, y) / (float)resolution;
            auto getPointOnSphere = [&](float px, float py) {
                glm::vec3 p = localUp + (px - 0.5f) * 2.0f * axisA + (py - 0.5f) * 2.0f * axisB;
                return glm::normalize(p);
            };

            glm::vec3 p0 = getPointOnSphere(percent.x, percent.y);
            float h0 = getElevationAt(p0, component);
            float finalRadius0 = std::max(component.radius + h0, component.seaLevel);
            glm::vec3 pos0 = p0 * finalRadius0;

            glm::vec3 n_sphere = p0; 
            glm::vec3 t1;
            if (std::abs(n_sphere.x) > 0.9f) t1 = glm::vec3(0, 1, 0);
            else t1 = glm::vec3(1, 0, 0);
            
            t1 = glm::normalize(glm::cross(t1, n_sphere));
            glm::vec3 t2 = glm::cross(n_sphere, t1);

            float adaptiveEpsilon = std::max(epsilon, 2.0f / (float)resolution);
            glm::vec3 p1 = glm::normalize(p0 + t1 * adaptiveEpsilon);
            glm::vec3 p2 = glm::normalize(p0 + t2 * adaptiveEpsilon);
            
            float h1 = getElevationAt(p1, component);
            float h2 = getElevationAt(p2, component);

            float finalRadius1 = std::max(component.radius + h1, component.seaLevel);
            float finalRadius2 = std::max(component.radius + h2, component.seaLevel);

            glm::vec3 pos1 = p1 * finalRadius1;
            glm::vec3 pos2 = p2 * finalRadius2;

            glm::vec3 normal;
            if (finalRadius0 <= component.seaLevel + 1e-4f && 
                finalRadius1 <= component.seaLevel + 1e-4f && 
                finalRadius2 <= component.seaLevel + 1e-4f) {
                normal = n_sphere;
            } else {
                normal = glm::normalize(glm::cross(pos1 - pos0, pos2 - pos0));
                if (glm::dot(normal, n_sphere) < 0) normal = -normal;
            }

            glm::vec3 color = getBiomeColorAt(p0, component);
            if (finalRadius0 <= component.seaLevel + 1e-5f) {
                color = glm::vec3(0.05f, 0.15f, 0.5f);
            }

            data.vertices[x + y * (resolution + 1)] = {
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
