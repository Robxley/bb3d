#pragma once

#include "bb3d/scene/Entity.hpp"
#include <glm/glm.hpp>
#include <algorithm>

namespace astrobazard {

/**
 * @brief Handles the Kerbal-style camera that orbits a planet, points "down" to the center, and zooms out with altitude.
 */
class SmartCamera {
public:
    /**
     * @param camera The camera entity to control.
     * @param planet The planet to orient "down" towards.
     * @param baseZoom Minimum zoom distance when strictly at the surface.
     */
    SmartCamera(bb3d::Entity camera, bb3d::Entity planet, float baseZoom = 20.0f);

    /**
     * @brief Updates the camera position, view matrix, and returns the computed scale factor for visually scaling objects.
     * @param target The target entity to follow (e.g. the spaceship).
     * @param altitude The current calculated altitude above the surface base.
     * @param gravityDir The current normalized direction of gravity (from target to planet center).
     * @param localOffset Local offset relative to the target's transform (e.g. to aim at the nose).
     * @return The scale factor to apply to the target for the Kerbal visual illusion.
     */
    float update(bb3d::Entity target, float altitude, const glm::vec3& gravityDir, const glm::vec3& localOffset = {0.0f, 0.0f, 0.0f});

    /**
     * @brief Adds a zoom offset (multiplier).
     */
    void zoom(float delta) { m_zoomFactor = std::clamp(m_zoomFactor - delta * 0.1f, 0.1f, 10.0f); }

private:
    bb3d::Entity m_camera;
    bb3d::Entity m_planet;
    float m_baseZoom;
    float m_zoomFactor = 1.0f;
};

} // namespace astrobazard
