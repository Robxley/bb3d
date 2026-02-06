#pragma once
#include "bb3d/scene/Camera.hpp"
#include <glm/glm.hpp>

namespace bb3d {

/**
 * @brief [COMPATIBILITY WRAPPER] Caméra First Person.
 * Utilisée par les anciens tests unitaires.
 */
class FPSCamera : public Camera {
public:
    FPSCamera(float fov, float aspect, float near, float far) : Camera(fov, aspect, near, far) {
        updateVectors();
    }

    FPSCamera(const glm::vec3& position, float fov = 45.0f, float aspect = 1.77f, float near = 0.1f, float far = 1000.0f) 
        : Camera(fov, aspect, near, far) {
        m_position = position;
        updateVectors();
    }

    void setRotation(float yaw, float pitch) {
        m_yaw = yaw;
        m_pitch = pitch;
        updateVectors();
    }

    void rotate(float deltaYaw, float deltaPitch) {
        m_yaw += deltaYaw * 0.1f;
        m_pitch += deltaPitch * 0.1f;
        updateVectors();
    }

    void move(const glm::vec3& direction, float amount) {
        m_position += direction * amount;
        updateVectors();
    }

    void update(float) override {
        updateVectors();
    }

private:
    void updateVectors() {
        glm::vec3 front;
        front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
        front.y = sin(glm::radians(m_pitch));
        front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
        glm::vec3 forward = glm::normalize(front);
        lookAt(m_position + forward);
    }

    float m_yaw = -90.0f;
    float m_pitch = 0.0f;
};

} // namespace bb3d