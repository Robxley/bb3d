#include "bb3d/scene/Camera.hpp"
#include <glm/gtc/matrix_transform.hpp>

namespace bb3d {

Camera::Camera(float fov, float aspect, float near, float far) {
    setPerspective(fov, aspect, near, far);
    updateViewMatrix();
}

void Camera::setPosition(const glm::vec3& position) {
    m_position = position;
    updateViewMatrix();
}

void Camera::lookAt(const glm::vec3& target) {
    m_front = glm::normalize(target - m_position);
    updateViewMatrix();
}

void Camera::setPerspective(float fov, float aspect, float near, float far) {
    m_proj = glm::perspective(glm::radians(fov), aspect, near, far);
    m_proj[1][1] *= -1; // Correction pour Vulkan (Y inversé)
}

void Camera::updateViewMatrix() {
    m_view = glm::lookAt(m_position, m_position + m_front, m_up);
}

CameraUniform Camera::getUniformData() const {
    return {
        m_view,
        m_proj,
        m_proj * m_view,
        m_position
    };
}

} // namespace bb3d
