#pragma once
#include "bb3d/scene/Camera.hpp"
#include <glm/gtc/quaternion.hpp>

namespace bb3d {

/**
 * @brief Caméra de type First Person Shooter (FPS).
 * 
 * Permet une rotation libre (Yaw/Pitch) et des déplacements relatifs à l'orientation.
 */
class FPSCamera : public Camera {
public:
    FPSCamera(float fov, float aspect, float near, float far);
    ~FPSCamera() override = default;

    Type getType() const override { return Type::FPS; }

    /** @brief Met à jour la matrice de vue. */
    void update(float deltaTime) override;

    /** @name Contrôles
     * @{
     */
    void move(const glm::vec3& direction, float deltaTime);
    void rotate(float yawOffset, float pitchOffset);
    /** @} */

    /** @name Setters
     * @{
     */
    void setRotation(float yaw, float pitch);
    void setMovementSpeed(float speed) { m_movementSpeed = speed; }
    void setSensitivity(float sensitivity) { m_mouseSensitivity = sensitivity; }
    /** @} */

private:
    void updateCameraVectors();

    glm::vec3 m_front{0.0f, 0.0f, -1.0f};
    glm::vec3 m_up{0.0f, 1.0f, 0.0f};
    glm::vec3 m_right{1.0f, 0.0f, 0.0f};
    glm::vec3 m_worldUp{0.0f, 1.0f, 0.0f};

    float m_yaw = -90.0f;
    float m_pitch = 0.0f;
    
    float m_movementSpeed = 5.0f;
    float m_mouseSensitivity = 0.1f;
};

} // namespace bb3d
