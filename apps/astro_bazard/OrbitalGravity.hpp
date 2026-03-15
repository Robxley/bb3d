#pragma once

#include "bb3d/scene/Entity.hpp"
#include "bb3d/core/Engine.hpp"
#include <glm/glm.hpp>

namespace astrobazard {

/**
 * @brief Computes and applies Newtonian gravity towards a target planet.
 */
class OrbitalGravity {
public:
    /**
     * @param planet The target celestial body to orbit.
     * @param gravityConstant The GM multiplier (e.g., 500,000 for AstroBazard).
     */
    OrbitalGravity(bb3d::Entity planet, float gravityConstant = 500000.0f);

    /**
     * @brief Applies gravitational force to the given entity.
     * @param target The entity that will be pulled (must have PhysicsComponent).
     * @param engine Pointer to the Engine for physics API access.
     */
    void update(bb3d::Entity target, bb3d::Engine* engine);

    [[nodiscard]] glm::vec3 getCurrentGravityDirection() const { return m_lastGravityDir; }

private:
    bb3d::Entity m_planet;
    float m_gravityConstant;
    glm::vec3 m_lastGravityDir{0.0f, -1.0f, 0.0f};
};

} // namespace astrobazard
