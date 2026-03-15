#include "SpaceshipController.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/physics/PhysicsWorld.hpp"

namespace astrobazard {

SpaceshipController::SpaceshipController(bb3d::Entity nose, float thrustPower, float retroThrustPower, float torquePower)
    : m_nose(nose), m_thrustPower(thrustPower), m_retroThrustPower(retroThrustPower), m_torquePower(torquePower) {
}

void SpaceshipController::update(bb3d::Entity ship, float deltaTime, bb3d::Engine* engine) {
    if (!ship.has<bb3d::TransformComponent>()) return;

    auto& tf = ship.get<bb3d::TransformComponent>();

    float thrustMag = 0.0f;
    float torqueMag = 0.0f;
    
    float currentThrust = m_thrustPower;
    float currentRetro = m_retroThrustPower;
    float currentTorque = m_torquePower;

    if (ship.has<bb3d::SpaceshipControllerComponent>()) {
        auto& ctrl = ship.get<bb3d::SpaceshipControllerComponent>();
        currentThrust = ctrl.mainThrustPower;
        currentRetro = ctrl.retroThrustPower;
        currentTorque = ctrl.torquePower;
    }
    
    if (engine->input().isKeyPressed(bb3d::Key::W) || engine->input().isKeyPressed(bb3d::Key::Up)) thrustMag += currentThrust;
    if (engine->input().isKeyPressed(bb3d::Key::S) || engine->input().isKeyPressed(bb3d::Key::Down)) thrustMag -= currentRetro;
    if (engine->input().isKeyPressed(bb3d::Key::A) || engine->input().isKeyPressed(bb3d::Key::Left)) torqueMag += currentTorque;
    if (engine->input().isKeyPressed(bb3d::Key::D) || engine->input().isKeyPressed(bb3d::Key::Right)) torqueMag -= currentTorque;
    
    // The spaceship 'Forward' is local Y axis since it points up initially
    glm::vec3 shipForward = {-std::sin(tf.rotation.z), std::cos(tf.rotation.z), 0.0f};
    
    if (thrustMag != 0.0f) {
        engine->physics().addForce(ship, shipForward * thrustMag);
    }
    if (torqueMag != 0.0f) {
        engine->physics().addTorque(ship, glm::vec3(0.0f, 0.0f, torqueMag));
    }

    // Particle Emission for Thrusters
    if (ship.has<bb3d::ParticleSystemComponent>()) {
        auto& ps = ship.get<bb3d::ParticleSystemComponent>();
        bb3d::ParticleProps props;
        props.lifeTime = 0.5f;
        props.colorBegin = {0.8f, 0.3f, 0.0f, 1.0f}; 
        props.colorEnd = {0.2f, 0.2f, 0.2f, 0.0f};   
        
        // Size scales with the current visual scale of the spaceship
        float s = tf.scale.x; 
        props.sizeBegin = 0.5f * s;
        props.sizeEnd = 1.5f * s;
        props.sizeVariation = 0.2f * s;
        
        glm::vec3 shipRight = {std::cos(tf.rotation.z), std::sin(tf.rotation.z), 0.0f};

        // Main & Retro Thrusters
        if (thrustMag > 0.0f) {
            props.position = tf.translation - shipForward * (0.6f * s);
            props.velocity = -shipForward * 5.0f * s;
            props.velocityVariation = {0.5f * s, 0.5f * s, 0.1f * s};
            ps.emit(props);
            ps.emit(props); // Double emission for main thruster
        } else if (thrustMag < 0.0f) {
            props.position = tf.translation + shipForward * (0.8f * s); // Adjusted position
            props.velocity = shipForward * 1.5f * s; // Slower velocity
            props.velocityVariation = {0.2f * s, 0.2f * s, 0.1f * s};
            props.colorBegin = {0.2f, 0.6f, 1.0f, 1.0f}; // Blue retro thrust
            props.sizeBegin = 0.2f * s;                  // Smaller particles
            props.sizeEnd = 0.6f * s;
            ps.emit(props);
        }
        
        // RCS Thrusters (Torque)
        if (torqueMag > 0.0f) { 
            // Left rotation -> fire right thruster to the right
            props.position = tf.translation + shipRight * (0.6f * s);
            props.velocity = shipRight * 1.5f * s;
            props.velocityVariation = {0.2f * s, 0.2f * s, 0.1f * s};
            props.colorBegin = {0.9f, 0.9f, 0.9f, 1.0f}; // White RCS
            props.sizeBegin = 0.15f * s;
            props.sizeEnd = 0.4f * s;
            ps.emit(props);
        } else if (torqueMag < 0.0f) { 
            // Right rotation -> fire left thruster to the left
            props.position = tf.translation - shipRight * (0.6f * s);
            props.velocity = -shipRight * 1.5f * s;
            props.velocityVariation = {0.2f * s, 0.2f * s, 0.1f * s};
            props.colorBegin = {0.9f, 0.9f, 0.9f, 1.0f}; // White RCS
            props.sizeBegin = 0.15f * s;
            props.sizeEnd = 0.4f * s;
            ps.emit(props);
        }
    }

    // Update Visual Nose to follow Body smoothly
    if (m_nose.has<bb3d::TransformComponent>()) {
        auto& noseTf = m_nose.get<bb3d::TransformComponent>();
        noseTf.translation = tf.translation + shipForward * 1.0f; // Offset by half cube + half cone
        noseTf.rotation = tf.rotation;
    }
}

void SpaceshipController::applyVisualScale(bb3d::Entity ship, float scaleFactor) {
    if (ship.has<bb3d::TransformComponent>()) {
        ship.get<bb3d::TransformComponent>().scale = { scaleFactor, scaleFactor, scaleFactor };
    }
    if (m_nose.has<bb3d::TransformComponent>()) {
        m_nose.get<bb3d::TransformComponent>().scale = { scaleFactor, scaleFactor, scaleFactor };
    }
}

} // namespace astrobazard
