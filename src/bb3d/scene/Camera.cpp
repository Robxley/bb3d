#include "bb3d/scene/Camera.hpp"

namespace bb3d {

Camera::Camera(float fov, float aspect, float near, float far) {
    setPerspective(fov, aspect, near, far);
}

void Camera::setPerspective(float fov, float aspect, float near, float far) {
    m_proj = glm::perspective(glm::radians(fov), aspect, near, far);
    m_proj[1][1] *= -1; // Correction Y pour Vulkan
}

void Camera::lookAt(const glm::vec3& target) {
    m_view = glm::lookAt(m_position, target, glm::vec3(0.0f, 1.0f, 0.0f));
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