#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <glm/glm.hpp>
#include "bb3d/core/Config.hpp"
#include <vulkan/vulkan.hpp>
#include <vector>
#include <array>

namespace bb3d {

struct VertexLayout {
    static constexpr uint32_t Position = 0;
    static constexpr uint32_t Normal   = 1;
    static constexpr uint32_t Color    = 2;
    static constexpr uint32_t UV       = 3;
    static constexpr uint32_t Tangent  = 4;
    
    // Réservé pour l'animation (Skinning)
    static constexpr uint32_t Joints   = 5;
    static constexpr uint32_t Weights  = 6;
};

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
    glm::vec2 uv;
    glm::vec4 tangent = {0.0f, 0.0f, 0.0f, 1.0f};
    glm::ivec4 joints = {0, 0, 0, 0};
    glm::vec4 weights = {0.0f, 0.0f, 0.0f, 0.0f};

    bool operator==(const Vertex& other) const {
        return position == other.position && normal == other.normal && color == other.color && uv == other.uv && tangent == other.tangent && joints == other.joints && weights == other.weights;
    }

    static vk::VertexInputBindingDescription getBindingDescription() {
        vk::VertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = vk::VertexInputRate::eVertex;
        return bindingDescription;
    }

    static std::array<vk::VertexInputAttributeDescription, 7> getAttributeDescriptions() {
        std::array<vk::VertexInputAttributeDescription, 7> attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = VertexLayout::Position;
        attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[0].offset = offsetof(Vertex, position);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = VertexLayout::Normal;
        attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[1].offset = offsetof(Vertex, normal);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = VertexLayout::Color;
        attributeDescriptions[2].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[2].offset = offsetof(Vertex, color);

        attributeDescriptions[3].binding = 0;
        attributeDescriptions[3].location = VertexLayout::UV;
        attributeDescriptions[3].format = vk::Format::eR32G32Sfloat;
        attributeDescriptions[3].offset = offsetof(Vertex, uv);

        attributeDescriptions[4].binding = 0;
        attributeDescriptions[4].location = VertexLayout::Tangent;
        attributeDescriptions[4].format = vk::Format::eR32G32B32A32Sfloat;
        attributeDescriptions[4].offset = offsetof(Vertex, tangent);

        attributeDescriptions[5].binding = 0;
        attributeDescriptions[5].location = VertexLayout::Joints;
        attributeDescriptions[5].format = vk::Format::eR32G32B32A32Sint;
        attributeDescriptions[5].offset = offsetof(Vertex, joints);

        attributeDescriptions[6].binding = 0;
        attributeDescriptions[6].location = VertexLayout::Weights;
        attributeDescriptions[6].format = vk::Format::eR32G32B32A32Sfloat;
        attributeDescriptions[6].offset = offsetof(Vertex, weights);

        return attributeDescriptions;
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
            size_t h5 = hash<glm::vec4>()(vertex.tangent);
            return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4);
        }
    };
}