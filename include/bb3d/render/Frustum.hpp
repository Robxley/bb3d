#pragma once

#include <glm/glm.hpp>
#include <array>

namespace bb3d {

/**
 * @brief ReprÃ©sente un volume de vue (frustum) pour le culling.
 */
class Frustum {
public:
    Frustum() = default;

    /** @brief Construit le frustum Ã  partir d'une matrice Vue-Projection. */
    Frustum(const glm::mat4& matrix) {
        // Extraction des plans (G, D, B, H, P, L)
        // BasÃ© sur la mÃ©thode de Fast Extraction of Viewing Frustum Planes from the World-View-Projection Matrix
        
        // Left
        m_planes[0] = { matrix[0][3] + matrix[0][0], matrix[1][3] + matrix[1][0], matrix[2][3] + matrix[2][0], matrix[3][3] + matrix[3][0] };
        // Right
        m_planes[1] = { matrix[0][3] - matrix[0][0], matrix[1][3] - matrix[1][0], matrix[2][3] - matrix[2][0], matrix[3][3] - matrix[3][0] };
        // Bottom
        m_planes[2] = { matrix[0][3] + matrix[0][1], matrix[1][3] + matrix[1][1], matrix[2][3] + matrix[2][1], matrix[3][3] + matrix[3][1] };
        // Top
        m_planes[3] = { matrix[0][3] - matrix[0][1], matrix[1][3] - matrix[1][1], matrix[2][3] - matrix[2][1], matrix[3][3] - matrix[3][1] };
        // Near
        m_planes[4] = { matrix[0][3] + matrix[0][2], matrix[1][3] + matrix[1][2], matrix[2][3] + matrix[2][2], matrix[3][3] + matrix[3][2] };
        // Far
        m_planes[5] = { matrix[0][3] - matrix[0][2], matrix[1][3] - matrix[1][2], matrix[2][3] - matrix[2][2], matrix[3][3] - matrix[3][2] };

        // Normalisation
        for (auto& plane : m_planes) {
            float length = glm::length(glm::vec3(plane));
            plane /= length;
        }
    }

    /** @brief Teste si une sphÃ¨re est dans le frustum. */
    bool isSphereVisible(const glm::vec3& center, float radius) const {
        for (const auto& plane : m_planes) {
            if (glm::dot(glm::vec3(plane), center) + plane.w < -radius) {
                return false;
            }
        }
        return true;
    }

    /** @brief Teste si une AABB est dans le frustum. */
    bool isAABBVisible(const glm::vec3& min, const glm::vec3& max) const {
        for (const auto& plane : m_planes) {
            glm::vec3 positive = min;
            if (plane.x >= 0) positive.x = max.x;
            if (plane.y >= 0) positive.y = max.y;
            if (plane.z >= 0) positive.z = max.z;

            if (glm::dot(glm::vec3(plane), positive) + plane.w < 0) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] const std::array<glm::vec4, 6>& getPlanes() const { return m_planes; }

private:
    std::array<glm::vec4, 6> m_planes;
};

} // namespace bb3d
