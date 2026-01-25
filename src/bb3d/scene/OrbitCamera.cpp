#include "bb3d/scene/OrbitCamera.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace bb3d {

OrbitCamera::OrbitCamera(float fov, float aspect, float near, float far)
    : Camera(fov, aspect, near, far) {
}

void OrbitCamera::update(float /*deltaTime*/) {
    // Spherical to Cartesian coordinates
    // pitch is elevation from XZ plane, yaw is rotation around Y
    float x = m_distance * std::cos(glm::radians(m_pitch)) * std::sin(glm::radians(m_yaw));
    float y = m_distance * std::sin(glm::radians(m_pitch));
    float z = m_distance * std::cos(glm::radians(m_pitch)) * std::cos(glm::radians(m_yaw));

    m_position = m_target + glm::vec3(x, y, z);
    m_view = glm::lookAt(m_position, m_target, glm::vec3(0.0f, 1.0f, 0.0f));
}

void OrbitCamera::rotate(float yawOffset, float pitchOffset) {
    m_yaw += yawOffset * m_mouseSensitivity;
    m_pitch += pitchOffset * m_mouseSensitivity;
    
    // Constraint pitch to avoid flipping and gimbal lock at poles
    m_pitch = std::clamp(m_pitch, -89.0f, 89.0f);
}

void OrbitCamera::zoom(float delta) {
    m_distance -= delta * m_zoomSpeed;
    m_distance = std::clamp(m_distance, m_minDistance, m_maxDistance);
}

void OrbitCamera::setTarget(const glm::vec3& target) {
    m_target = target;
}

} // namespace bb3d