#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <glm/glm.hpp>
#include "bb3d/core/Config.hpp"
#include <vulkan/vulkan.hpp>
#include <vector>
#include <array>

namespace bb3d {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
    glm::vec2 uv;

    static inline vk::VertexInputBindingDescription getBindingDescription() {
        return { 0, sizeof(Vertex), vk::VertexInputRate::eVertex };
    }

    static inline std::array<vk::VertexInputAttributeDescription, 4> getAttributeDescriptions() {
        std::array<vk::VertexInputAttributeDescription, 4> attributeDescriptions;

        // Position
        attributeDescriptions[0] = { EngineConfig::LAYOUT_LOCATION_POSITION, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position) };

        // Normal
        attributeDescriptions[1] = { EngineConfig::LAYOUT_LOCATION_NORMAL, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, normal) };

        // Color
        attributeDescriptions[2] = { EngineConfig::LAYOUT_LOCATION_COLOR, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color) };

        // UV
        attributeDescriptions[3] = { EngineConfig::LAYOUT_LOCATION_TEXCOORD, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, uv) };

        return attributeDescriptions;
    }

    bool operator==(const Vertex& other) const {
        return position == other.position && normal == other.normal && color == other.color && uv == other.uv;
    }
};

} // namespace bb3d

namespace std {
    template<> struct hash<bb3d::Vertex> {
        size_t operator()(bb3d::Vertex const& vertex) const {
            size_t h1 = hash<glm::vec3>()(vertex.position);
            size_t h2 = hash<glm::vec3>()(vertex.normal);
            size_t h3 = hash<glm::vec3>()(vertex.color);
            size_t h4 = hash<glm::vec2>()(vertex.uv);
            return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
        }
    };
}
