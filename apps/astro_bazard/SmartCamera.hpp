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
     * @param initialTarget The initial target entity to focus on.
     * @param baseZoom Minimum zoom distance when strictly at the surface.
     */
    SmartCamera(bb3d::Entity camera, bb3d::Entity initialTarget, float baseZoom = 20.0f);

    /**
     * @brief Sets a new target for the camera to follow (e.g. for COMM-LINK).
     */
    void setTarget(bb3d::Entity newTarget) { m_currentTarget = newTarget; }
    bb3d::Entity getTarget() const { return m_currentTarget; }

    /**
     * @brief Updates the camera position, view matrix, and returns the computed scale factor for visually scaling objects.
     * @param altitude The current calculated altitude above the surface base (can be 0 if target is planet).
     * @param gravityDir The current normalized direction of gravity (from target to planet center). Use [0,-1,0] if target is the planet itself.
     * @param localOffset Local offset relative to the target's transform (e.g. to aim at the nose).
     * @return The scale factor to apply to the target for the Kerbal visual illusion.
     */
    float update(float altitude, const glm::vec3& gravityDir, const glm::vec3& localOffset = {0.0f, 0.0f, 0.0f});

    /**
     * @brief Adds a zoom offset (multiplier).
     */
    // void zoom(float delta) { m_zoomFactor = std::clamp(m_zoomFactor - delta * 0.1f, 0.1f, 10.0f); }

private:
    bb3d::Entity m_camera;
    bb3d::Entity m_currentTarget;
    float m_baseZoom;
};

} // namespace astrobazard
