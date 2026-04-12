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
    bb3d::Entity getCamera() const { return m_camera; }

    /**
     * @brief Updates the camera position, view matrix algorithmically with smoothing.
     * @param dt Delta time for smooth interpolation.
     * @param manualZoomDelta Any scroll input from user to zoom in/out.
     * @param manualPitchDelta Mouse Y movement for pitch rotation.
     * @param manualYawDelta Mouse X movement for yaw rotation.
     * @param altitude The current calculated altitude above the surface base (can be 0 if target is planet).
     * @param gravityDir The current normalized direction of gravity (from target to planet center). 
     * @param localOffset Local offset relative to the target's transform.
     */
    void update(float dt, float manualZoomDelta, float manualPitchDelta, float manualYawDelta, float altitude, const glm::vec3& gravityDir, const glm::vec3& localOffset = {0.0f, 0.0f, 0.0f});

    /**
     * @brief Resets the smoothing flags so the camera snaps to the target on the next frame.
     */
    void resetSmoothing() { m_firstFrame = true; }

private:
    bb3d::Entity m_camera;
    bb3d::Entity m_currentTarget;
    
    // Zoom & Orbit control
    float m_baseZoom;
    float m_userZoomOffset = 0.0f;
    float m_pitch = 0.0f;
    float m_yaw = 0.0f;
    
    // Smoothing states
    bool m_firstFrame = true;
    glm::vec3 m_currentPosition{0.0f};
    glm::quat m_currentRotation{1.0f, 0.0f, 0.0f, 0.0f};
};

} // namespace astrobazard
