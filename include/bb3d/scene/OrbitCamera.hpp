#pragma once
#include "bb3d/scene/Camera.hpp"
#include <glm/glm.hpp>

namespace bb3d {

/**
 * @brief [COMPATIBILITY WRAPPER] Caméra Orbitale.
 * Utilisée par les anciens tests unitaires.
 */
class OrbitCamera : public Camera {
public:
    OrbitCamera(float fov, float aspect, float near, float far) : Camera(fov, aspect, near, far) {
        updatePosition();
    }

    void setTarget(const glm::vec3& target) {
        m_target = target;
        updatePosition();
    }

    void rotate(float deltaYaw, float deltaPitch) {
        m_yaw += deltaYaw * 0.1f;
        m_pitch += deltaPitch * 0.1f;
        updatePosition();
    }

    void zoom(float delta) {
        m_distance -= delta * 0.5f;
        if (m_distance < 0.1f) m_distance = 0.1f;
        updatePosition();
    }

    void update(float) override {
        updatePosition();
    }

private:
    void updatePosition() {
        float x = m_distance * std::cos(glm::radians(m_pitch)) * std::sin(glm::radians(m_yaw));
        float y = m_distance * std::sin(glm::radians(m_pitch));
        float z = m_distance * std::cos(glm::radians(m_pitch)) * std::cos(glm::radians(m_yaw));
        m_position = m_target + glm::vec3(x, y, z);
        lookAt(m_target);
    }

    glm::vec3 m_target{0.0f};
    float m_distance = 5.0f;
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
};

} // namespace bb3d