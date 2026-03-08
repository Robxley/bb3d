#pragma once

#include "bb3d/core/Base.hpp"
#include "bb3d/core/Log.hpp"
#include "bb3d/render/Buffer.hpp"
#include "bb3d/render/Vertex.hpp"
#include "bb3d/render/Texture.hpp"
#include "bb3d/render/Material.hpp"
#include <vector>
#include <limits>
#include <glm/glm.hpp>

namespace bb3d {

class VulkanContext;

/** @brief Axis-Aligned Bounding Box (AABB). */
struct AABB {
    glm::vec3 min{std::numeric_limits<float>::max()}; ///< Minimum corner (x, y, z).
    glm::vec3 max{std::numeric_limits<float>::lowest()}; ///< Maximum corner (x, y, z).

    /** @brief Extends the box to include a new point. */
    void extend(const glm::vec3& point) {
        min = glm::min(min, point);
        max = glm::max(max, point);
    }
    
    /** @brief Extends the box to include another AABB. */
    void extend(const AABB& other) {
        min = glm::min(min, other.min);
        max = glm::max(max, other.max);
    }

    [[nodiscard]] inline glm::vec3 center() const { return (min + max) * 0.5f; }
    [[nodiscard]] inline glm::vec3 size() const { return max - min; }

    /** @brief Calculates a new AABB after transformation by a matrix. */
    [[nodiscard]] inline AABB transform(const glm::mat4& m) const {
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
 * @brief Object containing geometric data ready for the GPU.
 * 
 * A Mesh consists of:
 * - A **Vertex Buffer** (Position, Normal, UV, etc.).
 * - An **Index Buffer** (Triangle indices for indexed drawing).
 * - A **Local AABB** for culling.
 * - An optional link to a **Material**.
 */
class Mesh {
public:
    /**
     * @brief Creates a mesh and transfers data to GPU memory.
     * @param context Vulkan context for buffer creation.
     * @param vertices List of vertices.
     * @param indices List of triangle indices.
     */
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

    ~Mesh() {
        m_vertexBuffer.reset();
        m_indexBuffer.reset();
    }

    /** @brief Retrieves the local spatial bounds of the mesh. */
    [[nodiscard]] inline const AABB& getBounds() const { return m_bounds; }

    /** 
     * @brief Records rendering commands for this mesh.
     * @param commandBuffer Active command buffer.
     * @param instanceCount Number of instances to draw (Instancing).
     * @param firstInstance Index of the first instance in the instance SSBO.
     */
    inline void draw(vk::CommandBuffer commandBuffer, uint32_t instanceCount = 1, uint32_t firstInstance = 0) const {
        vk::Buffer vbs[] = {m_vertexBuffer->getHandle()};
        vk::DeviceSize offsets[] = {0};
        commandBuffer.bindVertexBuffers(0, 1, vbs, offsets);
        commandBuffer.bindIndexBuffer(m_indexBuffer->getHandle(), 0, vk::IndexType::eUint32);
        commandBuffer.drawIndexed(m_indexCount, instanceCount, 0, 0, firstInstance);
    }

    /** @brief Updates GPU buffers after modifying the `getVertices()` list. */
    inline void updateVertices() {
#if defined(BB3D_DEBUG)
        if (m_cpuDataReleased) {
            BB_CORE_ERROR("Mesh: Cannot update vertices, CPU data has been released!");
            return;
        }
#endif
        m_vertexBuffer = Buffer::CreateVertexBuffer(m_context, m_vertices.data(), m_vertices.size() * sizeof(Vertex));
        m_bounds = AABB();
        for (const auto& v : m_vertices) {
            m_bounds.extend(v.position);
        }
    }

    /** 
     * @brief Releases RAM memory (vertices/indices) to save memory.
     * @note After this call, data is no longer available for the CPU (e.g., for Physics).
     * @warning Ensure that physics colliders have been created BEFORE calling this method.
     */
    void releaseCPUData() {
        if (m_cpuDataReleased) return;
        m_vertices.clear();
        m_vertices.shrink_to_fit();
        m_indices.clear();
        m_indices.shrink_to_fit();
        m_cpuDataReleased = true;
        BB_CORE_TRACE("Mesh: CPU data released to save RAM.");
    }

    [[nodiscard]] bool isCPUDataReleased() const { return m_cpuDataReleased; }

    /** @brief Retrieves vertices (Read-only). Logs an error if data has been released (Debug only). */
    const std::vector<Vertex>& getVertices() const { 
#if defined(BB3D_DEBUG)
        if (m_cpuDataReleased) BB_CORE_ERROR("Mesh: Accessing vertices after releaseCPUData()! Results will be empty.");
#endif
        return m_vertices; 
    }

    /** @brief Retrieves indices (Read-only). Logs an error if data has been released (Debug only). */
    const std::vector<uint32_t>& getIndices() const { 
#if defined(BB3D_DEBUG)
        if (m_cpuDataReleased) BB_CORE_ERROR("Mesh: Accessing indices after releaseCPUData()! Results will be empty.");
#endif
        return m_indices; 
    }

    /** @brief Retrieves vertices for modification. Logs an error if data has been released (Debug only). */
    std::vector<Vertex>& getVertices() { 
#if defined(BB3D_DEBUG)
        if (m_cpuDataReleased) BB_CORE_ERROR("Mesh: Accessing vertices for modification after releaseCPUData()!");
#endif
        return m_vertices; 
    }
    
    void setTexture(Ref<Texture> texture) { m_texture = texture; }
    Ref<Texture> getTexture() const { return m_texture; }

    void setMaterial(Ref<Material> material) { m_material = material; }
    Ref<Material> getMaterial() const { return m_material; }

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
    bool m_cpuDataReleased = false;
};

} // namespace bb3d
