#include "bb3d/scene/Camera.hpp"

namespace bb3d {

Camera::Camera(float fov, float aspect, float near, float far) 
    : m_fov(fov), m_aspect(aspect), m_near(near), m_far(far) {
    setPerspective(fov, aspect, near, far);
}

void Camera::setPerspective(float fov, float aspect, float near, float far) {
    m_fov = fov;
    m_aspect = aspect;
    m_near = near;
    m_far = far;
    m_proj = glm::perspective(glm::radians(fov), aspect, near, far);
    m_proj[1][1] *= -1; // Correction Y pour Vulkan
}

void Camera::setAspectRatio(float aspect) {
    if (m_aspect == aspect) return;
    setPerspective(m_fov, aspect, m_near, m_far);
}

void Camera::lookAt(const glm::vec3& target, const glm::vec3& up) {
    m_view = glm::lookAt(m_position, target, up);
}

void Camera::setViewMatrix(const glm::mat4& view) {
    m_view = view;
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
