#include "OrbitalGravity.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/physics/PhysicsWorld.hpp"

namespace astrobazard {

OrbitalGravity::OrbitalGravity(bb3d::Entity planet, float gravityConstant)
    : m_planet(planet), m_gravityConstant(gravityConstant) {
}

void OrbitalGravity::update(bb3d::Entity target, bb3d::Engine* engine) {
    if (!target.has<bb3d::TransformComponent>() || !m_planet.has<bb3d::TransformComponent>()) return;

    auto& tf = target.get<bb3d::TransformComponent>();
    glm::vec3 planetCenter = m_planet.get<bb3d::TransformComponent>().translation;
    
    float currentGravity = m_gravityConstant;
    if (target.has<bb3d::OrbitalGravityComponent>()) {
        currentGravity = target.get<bb3d::OrbitalGravityComponent>().strength;
    }

    glm::vec3 relativePos = tf.translation - planetCenter;
    float dist = glm::length(relativePos);
    
    // Prevent division by zero
    if (dist < 0.1f) dist = 0.1f;

    m_lastGravityDir = glm::normalize(-relativePos);
    
    // Pure Newton Gravity Force: F = m * GM / d^2
    // F = m * a  =>  a = GM / d^2  =>  F_target = m_target * a
    float targetMass = 1.0f;
    if (target.has<bb3d::PhysicsComponent>()) {
        targetMass = target.get<bb3d::PhysicsComponent>().mass;
    }

    float gravityAcceleration = currentGravity / (dist * dist);
    float gravityForceMag = gravityAcceleration * targetMass;
    glm::vec3 gravityForce = m_lastGravityDir * gravityForceMag;
    
    engine->physics().addForce(target, gravityForce);
}

} // namespace astrobazard
