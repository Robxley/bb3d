#pragma once
#include <glm/glm.hpp>

namespace bb3d {

struct CameraUniform {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewProj; // Pré-calculé
    glm::vec3 position;
};

class Camera {
public:
    Camera(float fov, float aspect, float near, float far);

    void setPosition(const glm::vec3& position);
    void setRotation(const glm::vec3& rotation); // Euler angles
    void lookAt(const glm::vec3& target);
    void setPerspective(float fov, float aspect, float near, float far);

    const glm::mat4& getViewMatrix() const { return m_view; }
    const glm::mat4& getProjectionMatrix() const { return m_proj; }
    const glm::vec3& getPosition() const { return m_position; }

    CameraUniform getUniformData() const;

private:
    void updateViewMatrix();

    glm::vec3 m_position{0.0f, 0.0f, 5.0f};
    glm::vec3 m_front{0.0f, 0.0f, -1.0f};
    glm::vec3 m_up{0.0f, 1.0f, 0.0f};
    
    glm::mat4 m_view{1.0f};
    glm::mat4 m_proj{1.0f};
};

} // namespace bb3d
