#pragma once

#include "bb3d/scene/Entity.hpp"
#include "bb3d/core/Engine.hpp"
#include <glm/glm.hpp>

namespace astrobazard {

/**
 * @brief Handles spaceship player inputs, propulsion, and visual assembly sync (nose).
 */
class SpaceshipController {
public:
    SpaceshipController(float thrustPower = 250.0f, float retroThrustPower = 100.0f, float torquePower = 20.0f);

    void update(bb3d::Entity ship, float deltaTime, bb3d::Engine* engine);

private:
    float m_thrustPower;
    float m_retroThrustPower;
    float m_torquePower;
};

} // namespace astrobazard
