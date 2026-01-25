#include "bb3d/render/MeshGenerator.hpp"
#include <glm/gtc/constants.hpp>

namespace bb3d {

Scope<Mesh> MeshGenerator::createCube(VulkanContext& context, float size, const glm::vec3& color) {
    float h = size * 0.5f;
    std::vector<Vertex> vertices = {
        // Front
        {{-h, -h,  h}, {0,0,1}, color, {0,0}}, {{ h, -h,  h}, {0,0,1}, color, {1,0}}, {{ h,  h,  h}, {0,0,1}, color, {1,1}}, {{-h,  h,  h}, {0,0,1}, color, {0,1}},
        // Back
        {{-h, -h, -h}, {0,0,-1}, color, {1,0}}, {{-h,  h, -h}, {0,0,-1}, color, {1,1}}, {{ h,  h, -h}, {0,0,-1}, color, {0,1}}, {{ h, -h, -h}, {0,0,-1}, color, {0,0}},
        // Top
        {{-h,  h, -h}, {0,1,0}, color, {0,1}}, {{-h,  h,  h}, {0,1,0}, color, {0,0}}, {{ h,  h,  h}, {0,1,0}, color, {1,0}}, {{ h,  h, -h}, {0,1,0}, color, {1,1}},
        // Bottom
        {{-h, -h, -h}, {0,-1,0}, color, {0,0}}, {{ h, -h, -h}, {0,-1,0}, color, {1,0}}, {{ h, -h,  h}, {0,-1,0}, color, {1,1}}, {{-h, -h,  h}, {0,-1,0}, color, {0,1}},
        // Right
        {{ h, -h, -h}, {1,0,0}, color, {1,0}}, {{ h,  h, -h}, {1,0,0}, color, {1,1}}, {{ h,  h,  h}, {1,0,0}, color, {0,1}}, {{ h, -h,  h}, {1,0,0}, color, {0,0}},
        // Left
        {{-h, -h, -h}, {-1,0,0}, color, {0,0}}, {{-h, -h,  h}, {-1,0,0}, color, {1,0}}, {{-h,  h,  h}, {-1,0,0}, color, {1,1}}, {{-h,  h, -h}, {-1,0,0}, color, {0,1}}
    };

    std::vector<uint32_t> indices;
    for (uint32_t i = 0; i < 6; i++) {
        uint32_t offset = i * 4;
        indices.push_back(offset + 0); indices.push_back(offset + 1); indices.push_back(offset + 2);
        indices.push_back(offset + 2); indices.push_back(offset + 3); indices.push_back(offset + 0);
    }

    return CreateScope<Mesh>(context, vertices, indices);
}

Scope<Mesh> MeshGenerator::createSphere(VulkanContext& context, float radius, uint32_t segments, const glm::vec3& color) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    for (uint32_t y = 0; y <= segments; y++) {
        for (uint32_t x = 0; x <= segments; x++) {
            float xSegment = (float)x / (float)segments;
            float ySegment = (float)y / (float)segments;
            float xPos = std::cos(xSegment * 2.0f * glm::pi<float>()) * std::sin(ySegment * glm::pi<float>());
            float yPos = std::cos(ySegment * glm::pi<float>());
            float zPos = std::sin(xSegment * 2.0f * glm::pi<float>()) * std::sin(ySegment * glm::pi<float>());

            glm::vec3 pos(xPos * radius, yPos * radius, zPos * radius);
            glm::vec3 normal(xPos, yPos, zPos);
            vertices.push_back({pos, normal, color, {xSegment, ySegment}});
        }
    }

    for (uint32_t y = 0; y < segments; y++) {
        for (uint32_t x = 0; x < segments; x++) {
            indices.push_back((y + 1) * (segments + 1) + x);
            indices.push_back(y * (segments + 1) + x);
            indices.push_back(y * (segments + 1) + (x + 1));

            indices.push_back((y + 1) * (segments + 1) + x);
            indices.push_back(y * (segments + 1) + (x + 1));
            indices.push_back((y + 1) * (segments + 1) + (x + 1));
        }
    }

    return CreateScope<Mesh>(context, vertices, indices);
}

Scope<Mesh> MeshGenerator::createCheckerboardPlane(VulkanContext& context, float size, uint32_t subdivisions, const glm::vec3& color1, const glm::vec3& color2) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    float step = size / (float)subdivisions;
    float start = -size * 0.5f;

    for (uint32_t z = 0; z < subdivisions; z++) {
        for (uint32_t x = 0; x < subdivisions; x++) {
            glm::vec3 color = ((x + z) % 2 == 0) ? color1 : color2;
            float xPos = start + (float)x * step;
            float zPos = start + (float)z * step;

            uint32_t offset = static_cast<uint32_t>(vertices.size());
            
            // On génère 4 sommets par carré pour avoir des couleurs nettes
            vertices.push_back({{xPos, 0, zPos + step}, {0,1,0}, color, {0,0}});
            vertices.push_back({{xPos + step, 0, zPos + step}, {0,1,0}, color, {1,0}});
            vertices.push_back({{xPos + step, 0, zPos}, {0,1,0}, color, {1,1}});
            vertices.push_back({{xPos, 0, zPos}, {0,1,0}, color, {0,1}});

            indices.push_back(offset + 0); indices.push_back(offset + 1); indices.push_back(offset + 2);
            indices.push_back(offset + 2); indices.push_back(offset + 3); indices.push_back(offset + 0);
        }
    }

    return CreateScope<Mesh>(context, vertices, indices);
}

} // namespace bb3d
