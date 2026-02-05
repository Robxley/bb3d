#pragma once

#include <glm/glm.hpp>
#include "bb3d/render/Mesh.hpp" // Pour AABB

namespace bb3d {

/**
 * @brief Représente une pyramide de vue (Frustum) composée de 6 plans.
 */
class Frustum {
public:
    Frustum() = default;

    /** @brief Extrait les plans du frustum à partir d'une matrice View-Projection. */
    void update(const glm::mat4& viewProj) {
        // Left
        m_plans[0] = { viewProj[0][3] + viewProj[0][0], viewProj[1][3] + viewProj[1][0], viewProj[2][3] + viewProj[2][0], viewProj[3][3] + viewProj[3][0] };
        // Right
        m_plans[1] = { viewProj[0][3] - viewProj[0][0], viewProj[1][3] - viewProj[1][0], viewProj[2][3] - viewProj[2][0], viewProj[3][3] - viewProj[3][0] };
        // Bottom
        m_plans[2] = { viewProj[0][3] + viewProj[0][1], viewProj[1][3] + viewProj[1][1], viewProj[2][3] + viewProj[2][1], viewProj[3][3] + viewProj[3][1] };
        // Top
        m_plans[3] = { viewProj[0][3] - viewProj[0][1], viewProj[1][3] - viewProj[1][1], viewProj[2][3] - viewProj[2][1], viewProj[3][3] - viewProj[3][1] };
        // Near
        m_plans[4] = { viewProj[0][3] + viewProj[0][2], viewProj[1][3] + viewProj[1][2], viewProj[2][3] + viewProj[2][2], viewProj[3][3] + viewProj[3][2] };
        // Far
        m_plans[5] = { viewProj[0][3] - viewProj[0][2], viewProj[1][3] - viewProj[1][2], viewProj[2][3] - viewProj[2][2], viewProj[3][3] - viewProj[3][2] };

        // Normalisation des plans
        for (int i = 0; i < 6; i++) {
            float length = glm::length(glm::vec3(m_plans[i]));
            m_plans[i] /= length;
        }
    }

    /** @brief Teste si une AABB est à l'intérieur ou intersecte le frustum. */
    bool intersects(const AABB& aabb) const {
        for (int i = 0; i < 6; i++) {
            glm::vec3 positive = aabb.min;
            if (m_plans[i].x >= 0) positive.x = aabb.max.x;
            if (m_plans[i].y >= 0) positive.y = aabb.max.y;
            if (m_plans[i].z >= 0) positive.z = aabb.max.z;

            if (glm::dot(glm::vec3(m_plans[i]), positive) + m_plans[i].w < 0) {
                return false; // Totalement à l'extérieur de ce plan
            }
        }
        return true;
    }

private:
    glm::vec4 m_plans[6];
};

} // namespace bb3d
