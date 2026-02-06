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

class VulkanContext;

/** @brief Boîte englobante alignée sur les axes (Axis-Aligned Bounding Box). */
struct AABB {
    glm::vec3 min{std::numeric_limits<float>::max()}; ///< Coin minimum (x, y, z).
    glm::vec3 max{std::numeric_limits<float>::lowest()}; ///< Coin maximum (x, y, z).

    /** @brief Étend la boîte pour inclure un nouveau point. */
    void extend(const glm::vec3& point) {
        min = glm::min(min, point);
        max = glm::max(max, point);
    }
    
    /** @brief Étend la boîte pour inclure une autre AABB. */
    void extend(const AABB& other) {
        min = glm::min(min, other.min);
        max = glm::max(max, other.max);
    }

    [[nodiscard]] inline glm::vec3 center() const { return (min + max) * 0.5f; }
    [[nodiscard]] inline glm::vec3 size() const { return max - min; }

    /** @brief Calcule une nouvelle AABB après transformation par une matrice. */
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
 * @brief Objet contenant les données géométriques prêtes pour le GPU.
 * 
 * Un maillage (Mesh) possède :
 * - Un **Vertex Buffer** (Tampon de sommets : Position, Normal, UV, etc.).
 * - Un **Index Buffer** (Tampon d'indices pour le dessin indexé).
 * - Une **AABB locale** pour le culling.
 * - Un lien optionnel vers un **Material**.
 */
class Mesh {
public:
    /**
     * @brief Crée un maillage et transfère les données vers la mémoire GPU.
     * @param context Contexte Vulkan pour la création des buffers.
     * @param vertices Liste des sommets.
     * @param indices Liste des indices de triangles.
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

    /** @brief Récupère les limites spatiales locales du maillage. */
    [[nodiscard]] inline const AABB& getBounds() const { return m_bounds; }

    /** 
     * @brief Enregistre les commandes de rendu pour ce maillage.
     * @param commandBuffer Buffer de commande actif.
     * @param instanceCount Nombre d'instances à dessiner (Instancing).
     * @param firstInstance Index du premier exemplaire dans le SSBO d'instances.
     */
    inline void draw(vk::CommandBuffer commandBuffer, uint32_t instanceCount = 1, uint32_t firstInstance = 0) const {
        vk::Buffer vbs[] = {m_vertexBuffer->getHandle()};
        vk::DeviceSize offsets[] = {0};
        commandBuffer.bindVertexBuffers(0, 1, vbs, offsets);
        commandBuffer.bindIndexBuffer(m_indexBuffer->getHandle(), 0, vk::IndexType::eUint32);
        commandBuffer.drawIndexed(m_indexCount, instanceCount, 0, 0, firstInstance);
    }

    /** @brief Met à jour les buffers GPU après modification de la liste `getVertices()`. */
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
     * @brief Libère la mémoire RAM (vertices/indices) pour économiser de la mémoire.
     * @note Après cet appel, les données ne sont plus disponibles pour le CPU (ex: pour la Physique).
     * @warning Assurez-vous que les colliders physiques ont été créés AVANT d'appeler cette méthode.
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

    /** @brief Récupère les sommets (Lecture seule). Log une erreur si les données ont été libérées (Debug uniquement). */
    const std::vector<Vertex>& getVertices() const { 
#if defined(BB3D_DEBUG)
        if (m_cpuDataReleased) BB_CORE_ERROR("Mesh: Accessing vertices after releaseCPUData()! Results will be empty.");
#endif
        return m_vertices; 
    }

    /** @brief Récupère les indices (Lecture seule). Log une erreur si les données ont été libérées (Debug uniquement). */
    const std::vector<uint32_t>& getIndices() const { 
#if defined(BB3D_DEBUG)
        if (m_cpuDataReleased) BB_CORE_ERROR("Mesh: Accessing indices after releaseCPUData()! Results will be empty.");
#endif
        return m_indices; 
    }

    /** @brief Récupère les sommets pour modification. Log une erreur si les données ont été libérées (Debug uniquement). */
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
