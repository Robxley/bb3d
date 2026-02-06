#include "bb3d/input/InputManager.hpp"
#include "bb3d/core/Log.hpp"
#include <SDL3/SDL.h>
#include <cstring>

namespace bb3d {

// Helper pour convertir notre enum Key en SDL_Scancode
static SDL_Scancode toSDLScancode(Key key) {
    return static_cast<SDL_Scancode>(key);
}

static int toSDLButton(Mouse button) {
    return static_cast<int>(button);
}

InputManager::InputManager() {
    std::memset(m_currentKeyState, 0, sizeof(m_currentKeyState));
    std::memset(m_previousKeyState, 0, sizeof(m_previousKeyState));
}

void InputManager::update() {
    // 1. Sauvegarder l'état précédent
    std::memcpy(m_previousKeyState, m_currentKeyState, sizeof(m_currentKeyState));
    m_previousMouseState = m_currentMouseState;
    glm::vec2 oldMousePos = m_currentMousePos;

    // 2. Récupérer le nouvel état du clavier
    int numKeys;
    const bool* state = SDL_GetKeyboardState(&numKeys);
    if (state) {
        // On sature à 512 pour éviter de déborder si SDL a plus de touches
        int count = (numKeys < 512) ? numKeys : 512;
        for (int i = 0; i < count; ++i) {
            m_currentKeyState[i] = state[i] ? 1 : 0;
        }
    }

    // 3. Récupérer le nouvel état de la souris
    float mx, my;
    m_currentMouseState = SDL_GetMouseState(&mx, &my);
    m_currentMousePos = {mx, my};
    m_mouseDelta = m_currentMousePos - oldMousePos;
}

void InputManager::clearDeltas() {
    // On reset le scroll au début de la frame
    m_mouseScroll = { 0.0f, 0.0f };
}

void InputManager::onEvent(const SDL_Event& event) {
    if (event.type == SDL_EVENT_MOUSE_WHEEL) {
        m_mouseScroll += glm::vec2(event.wheel.x, event.wheel.y);
    }
}

bool InputManager::isKeyPressed(Key key) const {
    SDL_Scancode scancode = toSDLScancode(key);
    if (static_cast<int>(scancode) >= 512) return false;
    return m_currentKeyState[scancode] != 0;
}

bool InputManager::isKeyJustPressed(Key key) const {
    SDL_Scancode scancode = toSDLScancode(key);
    if (static_cast<int>(scancode) >= 512) return false;
    return (m_currentKeyState[scancode] != 0) && (m_previousKeyState[scancode] == 0);
}

bool InputManager::isKeyJustReleased(Key key) const {
    SDL_Scancode scancode = toSDLScancode(key);
    if (static_cast<int>(scancode) >= 512) return false;
    return (m_currentKeyState[scancode] == 0) && (m_previousKeyState[scancode] != 0);
}

bool InputManager::isMouseButtonPressed(Mouse button) const {
    return (m_currentMouseState & SDL_BUTTON_MASK(toSDLButton(button))) != 0;
}

bool InputManager::isMouseButtonJustPressed(Mouse button) const {
    bool current = (m_currentMouseState & SDL_BUTTON_MASK(toSDLButton(button))) != 0;
    bool previous = (m_previousMouseState & SDL_BUTTON_MASK(toSDLButton(button))) != 0;
    return current && !previous;
}

bool InputManager::isMouseButtonJustReleased(Mouse button) const {
    bool current = (m_currentMouseState & SDL_BUTTON_MASK(toSDLButton(button))) != 0;
    bool previous = (m_previousMouseState & SDL_BUTTON_MASK(toSDLButton(button))) != 0;
    return !current && previous;
}

void InputManager::mapAction(std::string_view name, Key key) {
    m_actions[std::string(name)] = InputBinding(key);
    BB_CORE_TRACE("Input: Mapped action '{0}' to key {1}", name, static_cast<int>(key));
}

void InputManager::mapAction(std::string_view name, Mouse button) {
    m_actions[std::string(name)] = InputBinding(button);
    BB_CORE_TRACE("Input: Mapped action '{0}' to mouse button {1}", name, static_cast<int>(button));
}

void InputManager::mapAxis(std::string_view name, Key positive, Key negative) {
    m_axes[std::string(name)] = {positive, negative};
    BB_CORE_TRACE("Input: Mapped axis '{0}' (Pos:{1}, Neg:{2})", name, static_cast<int>(positive), static_cast<int>(negative));
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

bool InputManager::isActionJustPressed(std::string_view name) const {
    auto it = m_actions.find(std::string(name));
    if (it == m_actions.end()) return false;

    const auto& binding = it->second;
    if (binding.type == InputBinding::Type::Key) {
        return isKeyJustPressed(binding.value.key);
    } else if (binding.type == InputBinding::Type::MouseButton) {
        return isMouseButtonJustPressed(binding.value.mouse);
    }
    return false;
}

bool InputManager::isActionJustReleased(std::string_view name) const {
    auto it = m_actions.find(std::string(name));
    if (it == m_actions.end()) return false;

    const auto& binding = it->second;
    if (binding.type == InputBinding::Type::Key) {
        return isKeyJustReleased(binding.value.key);
    } else if (binding.type == InputBinding::Type::MouseButton) {
        return isMouseButtonJustReleased(binding.value.mouse);
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
