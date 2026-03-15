#include "SmartCamera.hpp"
#include "bb3d/scene/Components.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>

namespace astrobazard {

SmartCamera::SmartCamera(bb3d::Entity camera, bb3d::Entity planet, float baseZoom)
    : m_camera(camera), m_planet(planet), m_baseZoom(baseZoom) {
}

float SmartCamera::update(bb3d::Entity target, float altitude, const glm::vec3& gravityDir) {
    if (!m_camera.has<bb3d::CameraComponent>() || !target.has<bb3d::TransformComponent>()) return 1.0f;

    auto& camTf = m_camera.get<bb3d::TransformComponent>();
    auto& refCam = m_camera.get<bb3d::CameraComponent>().camera;
    auto& targetTf = target.get<bb3d::TransformComponent>();

    float targetZoom = (m_baseZoom + std::max(0.0f, altitude * 0.5f)) * m_zoomFactor;
    
    // The UP vector of the camera is opposite to gravity
    glm::vec3 camUp = -gravityDir;
    
    // Rotate the view matrix so UP matches camUp.
    // In a true 2D Kerbal view, the easiest way is to adjust the camera's rotation Euler angles
    float angle = std::atan2(-camUp.x, camUp.y);
    
    camTf.rotation.x = 0.0f;
    camTf.rotation.y = 0.0f;
    camTf.rotation.z = angle;
    // Position camera away on the Z axis relative to the target's current position
    camTf.translation = targetTf.translation + glm::vec3(0.0f, 0.0f, targetZoom); 

    // Apply this transform explicitly to the BB3D Camera object's view matrix!
    glm::mat4 camModel = glm::translate(glm::mat4(1.0f), camTf.translation) * glm::toMat4(glm::quat(camTf.rotation));
    if (refCam) {
        refCam->setViewMatrix(glm::inverse(camModel));
        refCam->setPosition(camTf.translation);
    }
    
    // Compute Shrink effect illusion scale factor
    return targetZoom / m_baseZoom;
}

} // namespace astrobazard
