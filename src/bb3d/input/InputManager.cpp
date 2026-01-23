#include "bb3d/input/InputManager.hpp"
#include <SDL3/SDL.h>

namespace bb3d {

// Helper pour convertir notre enum Key en SDL_Scancode
// Note: bb3d::Key suit le standard USB HID, tout comme SDL_Scancode.
// Un cast direct est donc possible pour la majorité des touches.
static SDL_Scancode toSDLScancode(Key key) {
    return static_cast<SDL_Scancode>(key);
}

static int toSDLButton(Mouse button) {
    return static_cast<int>(button);
}

void InputManager::update() {
    // SDL_PumpEvents est géré globalement, généralement dans Window::update() 
    // ou via SDL_PollEvent dans la boucle principale.
    // Si InputManager est utilisé de manière autonome sans boucle d'événements,
    // on pourrait appeler SDL_PumpEvents() ici.
    // Pour l'instant, on suppose que l'Engine gère la pompe.
}

bool InputManager::isKeyPressed(Key key) const {
    // SDL_GetKeyboardState retourne un pointeur interne géré par SDL.
    // Il ne faut pas le free.
    const bool* state = SDL_GetKeyboardState(nullptr);
    if (!state) return false;
    
    return state[toSDLScancode(key)];
}

bool InputManager::isMouseButtonPressed(Mouse button) const {
    float x, y;
    SDL_MouseButtonFlags mask = SDL_GetMouseState(&x, &y);
    return (mask & SDL_BUTTON_MASK(toSDLButton(button))) != 0;
}

glm::vec2 InputManager::getMousePosition() const {
    float x, y;
    SDL_GetMouseState(&x, &y);
    return {x, y};
}

void InputManager::mapAction(std::string_view name, Key key) {
    m_actions[std::string(name)] = InputBinding(key);
}

void InputManager::mapAction(std::string_view name, Mouse button) {
    m_actions[std::string(name)] = InputBinding(button);
}

void InputManager::mapAxis(std::string_view name, Key positive, Key negative) {
    m_axes[std::string(name)] = {positive, negative};
}

bool InputManager::isActionPressed(std::string_view name) const {
    auto it = m_actions.find(std::string(name));
    if (it == m_actions.end()) return false;

    const auto& binding = it->second;
    if (binding.type == InputBinding::Type::Key) {
        return isKeyPressed(binding.value.key);
    } else if (binding.type == InputBinding::Type::MouseButton) {
        return isMouseButtonPressed(binding.value.mouse);
    }
    return false;
}

float InputManager::getAxis(std::string_view name) const {
    auto it = m_axes.find(std::string(name));
    if (it == m_axes.end()) return 0.0f;

    const auto& binding = it->second;
    float val = 0.0f;
    if (isKeyPressed(binding.positive)) val += 1.0f;
    if (isKeyPressed(binding.negative)) val -= 1.0f;
    return val;
}

} // namespace bb3d
