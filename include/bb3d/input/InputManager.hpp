#pragma once

#include "bb3d/input/InputCodes.hpp"
#include <string>
#include <string_view>
#include <unordered_map>
#include <glm/vec2.hpp>

namespace bb3d {

struct InputBinding {
    enum class Type { Key, MouseButton, None } type = Type::None;
    union {
        Key key;
        Mouse mouse;
    } value;
    
    InputBinding() = default;
    InputBinding(Key k) : type(Type::Key) { value.key = k; }
    InputBinding(Mouse m) : type(Type::MouseButton) { value.mouse = m; }
};

struct AxisBinding {
    Key positive;
    Key negative;
};

class InputManager {
public:
    InputManager() = default;
    ~InputManager() = default;

    // Mise à jour de l'état (si nécessaire, ex: delta souris)
    void update(); 

    // --- Low Level Polling ---
    bool isKeyPressed(Key key) const;
    bool isMouseButtonPressed(Mouse button) const;
    glm::vec2 getMousePosition() const;

    // --- High Level Mapping ---
    void mapAction(std::string_view name, Key key);
    void mapAction(std::string_view name, Mouse button);
    void mapAxis(std::string_view name, Key positive, Key negative);

    // --- High Level Queries ---
    bool isActionPressed(std::string_view name) const;
    float getAxis(std::string_view name) const;

private:
    std::unordered_map<std::string, InputBinding> m_actions;
    std::unordered_map<std::string, AxisBinding> m_axes;
};

} // namespace bb3d
