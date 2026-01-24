#pragma once

#include "bb3d/core/Base.hpp"
#include "bb3d/render/Buffer.hpp"
#include "bb3d/render/Vertex.hpp"
#include "bb3d/render/Texture.hpp" // Pour le futur (Material)
#include <vector>
#include <limits> // FLT_MAX

namespace bb3d {

struct AABB {
    glm::vec3 min{std::numeric_limits<float>::max()};
    glm::vec3 max{std::numeric_limits<float>::lowest()};

    void extend(const glm::vec3& point) {
        min = glm::min(min, point);
        max = glm::max(max, point);
    }
    
    void extend(const AABB& other) {
        min = glm::min(min, other.min);
        max = glm::max(max, other.max);
    }

    glm::vec3 center() const { return (min + max) * 0.5f; }
    glm::vec3 size() const { return max - min; }

    AABB transform(const glm::mat4& m) const {
        glm::vec3 newMin = m[3];
        glm::vec3 newMax = m[3];
        
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                float a = m[j][i] * min[j];
                float b = m[j][i] * max[j];
                if (a < b) {
                    newMin[i] += a;
                    newMax[i] += b;
                } else {
                    newMin[i] += b;
                    newMax[i] += a;
                }
            }
        }
        return {newMin, newMax};
    }
};

class Mesh {
public:
    Mesh(VulkanContext& context, 
         const std::vector<Vertex>& vertices, 
         const std::vector<uint32_t>& indices)
         : m_indexCount(static_cast<uint32_t>(indices.size())) {
        
        m_vertexBuffer = Buffer::CreateVertexBuffer(context, vertices.data(), vertices.size() * sizeof(Vertex));
        m_indexBuffer = Buffer::CreateIndexBuffer(context, indices.data(), indices.size() * sizeof(uint32_t));

        // Calculate AABB
        for (const auto& v : vertices) {
            m_bounds.extend(v.position);
        }
    }

    ~Mesh() = default;

    const AABB& getBounds() const { return m_bounds; }

    void draw(VkCommandBuffer commandBuffer) const {
        VkBuffer vertexBuffers[] = {m_vertexBuffer->getHandle()};
        VkDeviceSize offsets[] = {0};
        
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer->getHandle(), 0, VK_INDEX_TYPE_UINT32);
        
        vkCmdDrawIndexed(commandBuffer, m_indexCount, 1, 0, 0, 0);
    }

    // TODO: Material support

private:
    Scope<Buffer> m_vertexBuffer;
    Scope<Buffer> m_indexBuffer;
    uint32_t m_indexCount;
    AABB m_bounds;
};

} // namespace bb3d
