#pragma once

#include "bb3d/core/Base.hpp"
#include "bb3d/render/Buffer.hpp"
#include "bb3d/render/Vertex.hpp"
#include "bb3d/render/Texture.hpp"
#include "bb3d/render/Material.hpp"
#include <vector>
#include <limits>
#include <glm/glm.hpp>

namespace bb3d {

/** @brief Boîte englobante alignée sur les axes (Axis-Aligned Bounding Box). */
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

    [[nodiscard]] inline glm::vec3 center() const { return (min + max) * 0.5f; }
    [[nodiscard]] inline glm::vec3 size() const { return max - min; }

    /** @brief Calcule une nouvelle AABB après transformation par une matrice. */
    [[nodiscard]] AABB transform(const glm::mat4& m) const {
        glm::vec3 newMin = m[3];
        glm::vec3 newMax = m[3];
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                float a = m[j][i] * min[j];
                float b = m[j][i] * max[j];
                if (a < b) {
                    newMin[i] += a; newMax[i] += b;
                } else {
                    newMin[i] += b; newMax[i] += a;
                }
            }
        }
        return {newMin, newMax};
    }
};

/**
 * @brief Représente une partie de géométrie indexée avec ses tampons GPU.
 */
class Mesh {
public:
    Mesh(VulkanContext& context, 
         const std::vector<Vertex>& vertices, 
         const std::vector<uint32_t>& indices)
         : m_context(context), m_vertices(vertices), m_indices(indices), m_indexCount(static_cast<uint32_t>(indices.size())) {
        
        m_vertexBuffer = Buffer::CreateVertexBuffer(context, m_vertices.data(), m_vertices.size() * sizeof(Vertex));
        m_indexBuffer = Buffer::CreateIndexBuffer(context, m_indices.data(), m_indices.size() * sizeof(uint32_t));

        for (const auto& v : vertices) {
            m_bounds.extend(v.position);
        }
    }

    ~Mesh() = default;

    /** @brief Récupère l'AABB locale du maillage. */
    [[nodiscard]] inline const AABB& getBounds() const { return m_bounds; }

    /** @brief Enregistre les commandes de rendu pour ce maillage. */
    inline void draw(vk::CommandBuffer commandBuffer) const {
        vk::Buffer vbs[] = {m_vertexBuffer->getHandle()};
        vk::DeviceSize offsets[] = {0};
        commandBuffer.bindVertexBuffers(0, 1, vbs, offsets);
        commandBuffer.bindIndexBuffer(m_indexBuffer->getHandle(), 0, vk::IndexType::eUint32);
        commandBuffer.drawIndexed(m_indexCount, 1, 0, 0, 0);
    }

    std::vector<Vertex>& getVertices() { return m_vertices; }
    
    void setTexture(Ref<Texture> texture) { m_texture = texture; }
    Ref<Texture> getTexture() const { return m_texture; }

    void setMaterial(Ref<Material> material) { m_material = material; }
    Ref<Material> getMaterial() const { return m_material; }

    void updateVertices() {
        // Re-création simple du buffer (Optimisation possible via Staging Buffer ou mapping)
        m_vertexBuffer = Buffer::CreateVertexBuffer(m_context, m_vertices.data(), m_vertices.size() * sizeof(Vertex));
        
        // Recalcul des bounds
        m_bounds = AABB();
        for (const auto& v : m_vertices) {
            m_bounds.extend(v.position);
        }
    }

private:
    VulkanContext& m_context;
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;
    Scope<Buffer> m_vertexBuffer;
    Scope<Buffer> m_indexBuffer;
    Ref<Texture> m_texture;
    Ref<Material> m_material;
    uint32_t m_indexCount;
    AABB m_bounds;
};

} // namespace bb3d