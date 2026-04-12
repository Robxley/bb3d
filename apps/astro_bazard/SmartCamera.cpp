#include "SmartCamera.hpp"
#include "bb3d/scene/Components.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>

namespace astrobazard {

SmartCamera::SmartCamera(bb3d::Entity camera, bb3d::Entity initialTarget, float baseZoom)
    : m_camera(camera), m_currentTarget(initialTarget), m_baseZoom(baseZoom) {
}

void SmartCamera::update(float dt, float manualZoomDelta, float manualPitchDelta, float manualYawDelta, float altitude, const glm::vec3& gravityDir, const glm::vec3& localOffset) {
    if (!m_camera.has<bb3d::CameraComponent>() || !m_currentTarget.has<bb3d::TransformComponent>()) return;

    auto& camTf = m_camera.get<bb3d::TransformComponent>();
    auto& refCam = m_camera.get<bb3d::CameraComponent>().camera;
    auto& targetTf = m_currentTarget.get<bb3d::TransformComponent>();
    
    float minZoom = -15.0f;
    float maxZoom = 200.0f;
    if (m_camera.has<bb3d::SmartCameraComponent>()) {
        auto& scm = m_camera.get<bb3d::SmartCameraComponent>();
        minZoom = scm.minZoom;
        maxZoom = scm.maxZoom;
    }

    // Zoom Logic
    m_userZoomOffset -= manualZoomDelta;
    m_userZoomOffset = std::clamp(m_userZoomOffset, minZoom, maxZoom);
    
    // Altitude framing base (gently scales out when very high)
    float altitudeZoomBase = std::max(0.0f, altitude * 0.5f);
    float targetZoom = m_baseZoom + altitudeZoomBase + m_userZoomOffset;
    
    // Target Rotation Base
    glm::vec3 camUp = -gravityDir;
    float angle = std::atan2(-camUp.x, camUp.y);
    glm::quat baseRot = glm::quat(glm::vec3(0.0f, 0.0f, angle));

    // Mouse Orbit Rotation
    m_pitch += manualPitchDelta * 0.2f;
    m_yaw   += manualYawDelta * 0.2f;
    m_pitch = std::clamp(m_pitch, -85.0f, 85.0f);
    
    glm::quat orbitRot = glm::quat(glm::vec3(glm::radians(m_pitch), glm::radians(m_yaw), 0.0f));
    glm::quat targetRot = baseRot * orbitRot;

    // Target Position Base
    glm::vec3 worldOffset = glm::rotate(glm::quat(targetTf.rotation), localOffset);
    glm::vec3 lookAtTarget = targetTf.translation + worldOffset;
    
    // Position the camera backward along its local forward direction
    glm::vec3 cameraForward = targetRot * glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 targetPos = lookAtTarget - cameraForward * targetZoom; 

    if (m_firstFrame) {
        m_currentPosition = targetPos;
        m_currentRotation = targetRot;
        m_firstFrame = false;
    } else {
        float rotDamping = 5.0f;
        
        // Strict mapping on position to prevent physics/lerp micro-jitter on the ship
        m_currentPosition = targetPos; 
        
        // Soft mapping on rotation for the cinematic horizon alignment
        m_currentRotation = glm::slerp(m_currentRotation, targetRot, 1.0f - std::exp(-rotDamping * dt));
    }

    camTf.translation = m_currentPosition;
    camTf.rotation = glm::eulerAngles(m_currentRotation);

    // Camera logic modes:
    // 0 = Dolly Zoom (FOV reduction, keep camera at baseDistance)
    // 1 = Realistic (Camera pulls back, physical FOV=60)
    // 2 = Visual Scale (Camera pulls back, physical FOV=60 - scale is handled in explicit game logic)
    
    int mode = 2;
    if (m_camera.has<bb3d::SmartCameraComponent>()) {
        mode = m_camera.get<bb3d::SmartCameraComponent>().mode;
    }

    auto& camComp = m_camera.get<bb3d::CameraComponent>();
    float baseFov = 60.0f;
    float baseDistance = m_baseZoom + m_userZoomOffset;

    if (mode == 0) {
        // Mode 0: Dolly Zoom (Clamp targetPos distance internally by not using targetZoom for targetPos!)
        // Wait, m_currentPosition is already set using targetZoom!
        // We will just change the FOV assuming targetZoom is the physical distance.
        float distanceRatio = baseDistance / std::max(0.1f, targetZoom);
        float targetFov = 2.0f * std::atan(distanceRatio * std::tan(glm::radians(baseFov) * 0.5f));
        camComp.fov = std::clamp(glm::degrees(targetFov), 2.0f, 120.0f);
    } else {
        // Mode 1 and 2: Realistic and KSP (FOV stays locked at 60)
        camComp.fov = baseFov;
    }

    if (refCam) {
        refCam->setPerspective(camComp.fov, camComp.aspect, camComp.nearPlane, camComp.farPlane);
    }

    // Apply this transform explicitly to the BB3D Camera object's view matrix
    glm::mat4 camModel = glm::translate(glm::mat4(1.0f), m_currentPosition) * glm::toMat4(m_currentRotation);
    if (refCam) {
        refCam->setViewMatrix(glm::inverse(camModel));
        refCam->setPosition(m_currentPosition);
    }
}

} // namespace astrobazard
