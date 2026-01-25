#include "bb3d/scene/FPSCamera.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace bb3d {

FPSCamera::FPSCamera(float fov, float aspect, float near, float far)
    : Camera(fov, aspect, near, far) {
    updateCameraVectors();
}

void FPSCamera::update(float /*deltaTime*/) {
    m_view = glm::lookAt(m_position, m_position + m_front, m_up);
}

void FPSCamera::move(const glm::vec3& direction, float deltaTime) {
    float velocity = m_movementSpeed * deltaTime;
    m_position += m_front * direction.z * velocity;
    m_position += m_right * direction.x * velocity;
    m_position += m_up * direction.y * velocity;
}

void FPSCamera::rotate(float yawOffset, float pitchOffset) {
    m_yaw += yawOffset * m_mouseSensitivity;
    m_pitch += pitchOffset * m_mouseSensitivity;

    // Limiter le pitch pour éviter le retournement
    m_pitch = std::clamp(m_pitch, -89.0f, 89.0f);

    updateCameraVectors();
}

void FPSCamera::setRotation(float yaw, float pitch) {
    m_yaw = yaw;
    m_pitch = pitch;
    updateCameraVectors();
}

void FPSCamera::updateCameraVectors() {
    glm::vec3 front;
    front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    front.y = sin(glm::radians(m_pitch));
    front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    
    m_front = glm::normalize(front);
    m_right = glm::normalize(glm::cross(m_front, m_worldUp));
    m_up    = glm::normalize(glm::cross(m_right, m_front));
}

} // namespace bb3d
