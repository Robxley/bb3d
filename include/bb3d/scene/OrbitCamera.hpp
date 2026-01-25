#pragma once
#include "bb3d/scene/Camera.hpp"

namespace bb3d {

/**
 * @brief Caméra orbitale qui tourne autour d'un point cible.
 */
class OrbitCamera : public Camera {
public:
    OrbitCamera(float fov, float aspect, float near, float far);
    ~OrbitCamera() override = default;

    /** @brief Calcule la position de la caméra en fonction de l'orbite. */
    void update(float deltaTime) override;

    /** @name Contrôles
     * @{
     */
    void rotate(float yawOffset, float pitchOffset);
    void zoom(float delta);
    void setTarget(const glm::vec3& target);
    /** @} */

private:
    glm::vec3 m_target{0.0f, 0.0f, 0.0f};
    float m_distance = 5.0f;
    float m_minDistance = 1.0f;
    float m_maxDistance = 100.0f;

    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    
    float m_mouseSensitivity = 0.1f;
    float m_zoomSpeed = 0.5f;
};

} // namespace bb3d
