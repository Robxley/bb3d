#pragma once

#include <glm/glm.hpp>
#include <array>
#include "bb3d/render/Mesh.hpp" // Pour AABB

namespace bb3d {

/**
 * @brief Représente une pyramide de vue (Frustum) composée de 6 plans pour le culling.
 * 
 * Cette classe utilise les conventions Vulkan (Z: 0 à 1) pour l'extraction des plans.
 */
class Frustum {
public:
    Frustum() = default;

    /**
     * @brief Extrait les plans du frustum à partir d'une matrice View-Projection.
     * @param viewProj Matrice combinée Vue * Projection.
     */
    void update(const glm::mat4& viewProj) {
        // Left
        m_planes[0] = { viewProj[0][3] + viewProj[0][0], viewProj[1][3] + viewProj[1][0], viewProj[2][3] + viewProj[2][0], viewProj[3][3] + viewProj[3][0] };
        // Right
        m_planes[1] = { viewProj[0][3] - viewProj[0][0], viewProj[1][3] - viewProj[1][0], viewProj[2][3] - viewProj[2][0], viewProj[3][3] - viewProj[3][0] };
        // Bottom
        m_planes[2] = { viewProj[0][3] + viewProj[0][1], viewProj[1][3] + viewProj[1][1], viewProj[2][3] + viewProj[2][1], viewProj[3][3] + viewProj[3][1] };
        // Top
        m_planes[3] = { viewProj[0][3] - viewProj[0][1], viewProj[1][3] - viewProj[1][1], viewProj[2][3] - viewProj[2][1], viewProj[3][3] - viewProj[3][1] };
        // Near (z = 0 in clip space for Vulkan)
        m_planes[4] = { viewProj[0][2], viewProj[1][2], viewProj[2][2], viewProj[3][2] };
        // Far (z = w in clip space)
        m_planes[5] = { viewProj[0][3] - viewProj[0][2], viewProj[1][3] - viewProj[1][2], viewProj[2][3] - viewProj[2][2], viewProj[3][3] - viewProj[3][2] };

        // Normalisation des plans pour que dot(plane.xyz, point) + plane.w soit la distance réelle.
        for (auto& plane : m_planes) {
            float length = glm::length(glm::vec3(plane));
            plane /= length;
        }
    }

    /**
     * @brief Teste si une AABB est à l'intérieur ou intersecte le frustum.
     * @param aabb Boîte englobante alignée sur les axes.
     * @return true si l'objet est potentiellement visible, false s'il est totalement hors champ.
     */
    bool intersects(const AABB& aabb) const {
        for (const auto& plane : m_planes) {
            // On cherche le point "le plus positif" (p-vertex) dans la direction de la normale du plan.
            // Si ce point est derrière le plan, toute la boîte est derrière.
            glm::vec3 positive = aabb.min;
            if (plane.x >= 0) positive.x = aabb.max.x;
            if (plane.y >= 0) positive.y = aabb.max.y;
            if (plane.z >= 0) positive.z = aabb.max.z;

            if (glm::dot(glm::vec3(plane), positive) + plane.w < 0) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Teste si une sphère est à l'intérieur ou intersecte le frustum.
     * @param center Centre de la sphère.
     * @param radius Rayon de la sphère.
     * @return true si visible.
     */
    bool intersects(const glm::vec3& center, float radius) const {
        for (const auto& plane : m_planes) {
            if (glm::dot(glm::vec3(plane), center) + plane.w < -radius) {
                return false;
            }
        }
        return true;
    }

    /** @brief Récupère les 6 plans du frustum (xyz = normale, w = distance à l'origine). */
    [[nodiscard]] const std::array<glm::vec4, 6>& getPlanes() const { return m_planes; }

private:
    std::array<glm::vec4, 6> m_planes;
};

} // namespace bb3d
