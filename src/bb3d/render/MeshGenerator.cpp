#include "bb3d/render/MeshGenerator.hpp"
#include <glm/gtc/constants.hpp>

namespace bb3d {

Scope<Mesh> MeshGenerator::createCube(VulkanContext& context, float size, const glm::vec3& color) {
    float h = size * 0.5f;
    std::vector<Vertex> vertices = {
        // Front (Normal 0,0,1) -> Tangent (1,0,0)
        {{-h, -h,  h}, {0,0,1}, color, {0,0}, {1,0,0,1}}, {{ h, -h,  h}, {0,0,1}, color, {1,0}, {1,0,0,1}}, {{ h,  h,  h}, {0,0,1}, color, {1,1}, {1,0,0,1}}, {{-h,  h,  h}, {0,0,1}, color, {0,1}, {1,0,0,1}},
        // Back (Normal 0,0,-1) -> Tangent (-1,0,0)
        {{-h, -h, -h}, {0,0,-1}, color, {1,0}, {-1,0,0,1}}, {{-h,  h, -h}, {0,0,-1}, color, {1,1}, {-1,0,0,1}}, {{ h,  h, -h}, {0,0,-1}, color, {0,1}, {-1,0,0,1}}, {{ h, -h, -h}, {0,0,-1}, color, {0,0}, {-1,0,0,1}},
        // Top (Normal 0,1,0) -> Tangent (1,0,0)
        {{-h,  h, -h}, {0,1,0}, color, {0,1}, {1,0,0,1}}, {{-h,  h,  h}, {0,1,0}, color, {0,0}, {1,0,0,1}}, {{ h,  h,  h}, {0,1,0}, color, {1,0}, {1,0,0,1}}, {{ h,  h, -h}, {0,1,0}, color, {1,1}, {1,0,0,1}},
        // Bottom (Normal 0,-1,0) -> Tangent (1,0,0)
        {{-h, -h, -h}, {0,-1,0}, color, {0,0}, {1,0,0,1}}, {{ h, -h, -h}, {0,-1,0}, color, {1,0}, {1,0,0,1}}, {{ h, -h,  h}, {0,-1,0}, color, {1,1}, {1,0,0,1}}, {{-h, -h,  h}, {0,-1,0}, color, {0,1}, {1,0,0,1}},
        // Right (Normal 1,0,0) -> Tangent (0,0,-1)
        {{ h, -h, -h}, {1,0,0}, color, {1,0}, {0,0,-1,1}}, {{ h,  h, -h}, {1,0,0}, color, {1,1}, {0,0,-1,1}}, {{ h,  h,  h}, {1,0,0}, color, {0,1}, {0,0,-1,1}}, {{ h, -h,  h}, {1,0,0}, color, {0,0}, {0,0,-1,1}},
        // Left (Normal -1,0,0) -> Tangent (0,0,1)
        {{-h, -h, -h}, {-1,0,0}, color, {0,0}, {0,0,1,1}}, {{-h, -h,  h}, {-1,0,0}, color, {1,0}, {0,0,1,1}}, {{-h,  h,  h}, {-1,0,0}, color, {1,1}, {0,0,1,1}}, {{-h,  h, -h}, {-1,0,0}, color, {0,1}, {0,0,1,1}}
    };

    std::vector<uint32_t> indices;
    for (uint32_t i = 0; i < 6; i++) {
        uint32_t offset = i * 4;
        indices.push_back(offset + 0); indices.push_back(offset + 1); indices.push_back(offset + 2);
        indices.push_back(offset + 2); indices.push_back(offset + 3); indices.push_back(offset + 0);
    }

    return CreateScope<Mesh>(context, vertices, indices);
}

Scope<Mesh> MeshGenerator::createWireframeCube(VulkanContext& context, float size, const glm::vec3& color) {
    float h = size * 0.5f;

    // 8 corners of the cube
    std::vector<Vertex> vertices = {
        {{-h, -h,  h}, {0,0,0}, color, {0,0}, {0,0,0,0}}, // 0
        {{ h, -h,  h}, {0,0,0}, color, {0,0}, {0,0,0,0}}, // 1
        {{ h,  h,  h}, {0,0,0}, color, {0,0}, {0,0,0,0}}, // 2
        {{-h,  h,  h}, {0,0,0}, color, {0,0}, {0,0,0,0}}, // 3
        {{-h, -h, -h}, {0,0,0}, color, {0,0}, {0,0,0,0}}, // 4
        {{ h, -h, -h}, {0,0,0}, color, {0,0}, {0,0,0,0}}, // 5
        {{ h,  h, -h}, {0,0,0}, color, {0,0}, {0,0,0,0}}, // 6
        {{-h,  h, -h}, {0,0,0}, color, {0,0}, {0,0,0,0}}  // 7
    };

    // 12 lines (24 indices)
    std::vector<uint32_t> indices = {
        // Front face
        0, 1,  1, 2,  2, 3,  3, 0,
        // Back face
        4, 5,  5, 6,  6, 7,  7, 4,
        // Connecting lines
        0, 4,  1, 5,  2, 6,  3, 7
    };

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
            
            // Tangent calculation for sphere
            float theta = xSegment * 2.0f * glm::pi<float>();
            glm::vec3 tangent(-std::sin(theta), 0.0f, std::cos(theta));
            
            vertices.push_back({pos, normal, color, {xSegment, ySegment}, glm::vec4(tangent, 1.0f)});
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

Scope<Mesh> MeshGenerator::createCone(VulkanContext& context, float radius, float height, uint32_t segments, const glm::vec3& color) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    float halfHeight = height * 0.5f;

    // 1. Center of the base (bottom)
    Vertex bottomCenter;
    bottomCenter.position = {0.0f, -halfHeight, 0.0f};
    bottomCenter.normal = {0.0f, -1.0f, 0.0f};
    bottomCenter.color = color;
    bottomCenter.uv = {0.5f, 0.5f};
    bottomCenter.tangent = {1.0f, 0.0f, 0.0f, 1.0f};
    vertices.push_back(bottomCenter);
    uint32_t bottomCenterIndex = 0;

    // 2. Base perimeter vertices (for both the bottom cap and the side wall)
    // We duplicate the perimeter vertices so normals can differ (flat for bottom, sloped for side)
    
    // Bottom cap perimeter
    uint32_t baseCapStart = 1;
    for (uint32_t i = 0; i <= segments; ++i) {
        float theta = (float)i / (float)segments * 2.0f * glm::pi<float>();
        float c = std::cos(theta);
        float s = std::sin(theta);
        
        Vertex v;
        v.position = {c * radius, -halfHeight, s * radius};
        v.normal = {0.0f, -1.0f, 0.0f};
        v.color = color;
        v.uv = {c * 0.5f + 0.5f, s * 0.5f + 0.5f};
        v.tangent = {1.0f, 0.0f, 0.0f, 1.0f};
        vertices.push_back(v);
    }

    // Indices for bottom cap
    for (uint32_t i = 0; i < segments; ++i) {
        indices.push_back(bottomCenterIndex);
        indices.push_back(baseCapStart + i + 1);
        indices.push_back(baseCapStart + i);
    }

    // 3. Side wall vertices
    uint32_t sideWallStart = static_cast<uint32_t>(vertices.size());
    
    // The slope of the cone:
    float slopeY = radius / std::sqrt(radius*radius + height*height);
    float slopeXZ = height / std::sqrt(radius*radius + height*height);

    for (uint32_t i = 0; i <= segments; ++i) {
        float theta = (float)i / (float)segments * 2.0f * glm::pi<float>();
        float c = std::cos(theta);
        float s = std::sin(theta);

        glm::vec3 normal(c * slopeXZ, slopeY, s * slopeXZ);
        glm::vec3 tangent(-s, 0.0f, c);

        // Base vertex for side wall
        Vertex vBase;
        vBase.position = {c * radius, -halfHeight, s * radius};
        vBase.normal = normal;
        vBase.color = color;
        vBase.uv = {(float)i / segments, 1.0f};
        vBase.tangent = glm::vec4(tangent, 1.0f);
        vertices.push_back(vBase);

        // Tip vertex for side wall (each needs its own normal for smooth shading!)
        Vertex vTip;
        vTip.position = {0.0f, halfHeight, 0.0f};
        vTip.normal = normal; // Technically all converge to tip, but average normal works.
        vTip.color = color;
        vTip.uv = {(float)i / segments, 0.0f};
        vTip.tangent = glm::vec4(tangent, 1.0f);
        vertices.push_back(vTip);
    }

    // Indices for side wall
    for (uint32_t i = 0; i < segments; ++i) {
        uint32_t base1 = sideWallStart + i * 2;
        uint32_t tip1 = sideWallStart + i * 2 + 1;
        uint32_t base2 = sideWallStart + (i + 1) * 2;
        // uint32_t tip2 = sideWallStart + (i + 1) * 2 + 1;

        indices.push_back(base1);
        indices.push_back(base2);
        indices.push_back(tip1); // Tip connects the two bases.
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
            
            // Generate 4 vertices per square for sharp colors
            vertices.push_back({{xPos, 0, zPos + step}, {0,1,0}, color, {0,0}, {1,0,0,1}});
            vertices.push_back({{xPos + step, 0, zPos + step}, {0,1,0}, color, {1,0}, {1,0,0,1}});
            vertices.push_back({{xPos + step, 0, zPos}, {0,1,0}, color, {1,1}, {1,0,0,1}});
            vertices.push_back({{xPos, 0, zPos}, {0,1,0}, color, {0,1}, {1,0,0,1}});

            indices.push_back(offset + 0); indices.push_back(offset + 1); indices.push_back(offset + 2);
            indices.push_back(offset + 2); indices.push_back(offset + 3); indices.push_back(offset + 0);
        }
    }

    return CreateScope<Mesh>(context, vertices, indices);
}

Scope<Mesh> MeshGenerator::createQuad(VulkanContext& context, float size, const glm::vec3& color) {
    float h = size * 0.5f;
    std::vector<Vertex> vertices = {
        {{-h, -h, 0.0f}, {0,0,1}, color, {0,1}, {1,0,0,1}},
        {{ h, -h, 0.0f}, {0,0,1}, color, {1,1}, {1,0,0,1}},
        {{ h,  h, 0.0f}, {0,0,1}, color, {1,0}, {1,0,0,1}},
        {{-h,  h, 0.0f}, {0,0,1}, color, {0,0}, {1,0,0,1}}
    };

    std::vector<uint32_t> indices = { 0, 1, 2, 2, 3, 0 };

    return CreateScope<Mesh>(context, vertices, indices);
}

} // namespace bb3d