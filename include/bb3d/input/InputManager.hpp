#pragma once

#include "bb3d/input/InputCodes.hpp"
#include <string>
#include <string_view>
#include <unordered_map>
#include <glm/vec2.hpp>

// Forward declare SDL_Event in the global namespace
union SDL_Event;

namespace bb3d {

/** @brief Structure interne pour lier une action à une touche ou un bouton. */
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

/** @brief Structure pour lier un axe à deux touches (ex: Horizontal -> Q/D). */
struct AxisBinding {
    Key positive;
    Key negative;
};

/**
 * @brief Gère les entrées utilisateur et le mapping d'actions.
 */
class InputManager {
public:
    InputManager() = default;
    ~InputManager() = default;

    /** @brief Met à jour l'état interne (reset deltas). */
    void update(); 

    /** @brief Traite un événement SDL entrant. */
    void onEvent(const SDL_Event& event);

    /** @name Polling Bas Niveau
     * @{
     */
    [[nodiscard]] bool isKeyPressed(Key key) const;
    [[nodiscard]] bool isMouseButtonPressed(Mouse button) const;
    [[nodiscard]] glm::vec2 getMousePosition() const { return m_mousePos; }
    [[nodiscard]] glm::vec2 getMouseDelta() const { return m_mouseDelta; }
    [[nodiscard]] glm::vec2 getMouseScroll() const { return m_mouseScroll; }
    /** @} */

    /** @name Mapping Haut Niveau
     * @{
     */
    void mapAction(std::string_view name, Key key);
    void mapAction(std::string_view name, Mouse button);
    void mapAxis(std::string_view name, Key positive, Key negative);
    /** @} */

    /** @name Requêtes d'Actions
     * @{
     */
    [[nodiscard]] bool isActionPressed(std::string_view name) const;
    [[nodiscard]] float getAxis(std::string_view name) const;
    /** @} */

private:
    std::unordered_map<std::string, InputBinding> m_actions;
    std::unordered_map<std::string, AxisBinding> m_axes;

    glm::vec2 m_mousePos{ 0.0f, 0.0f };
    glm::vec2 m_mouseDelta{ 0.0f, 0.0f };
    glm::vec2 m_mouseScroll{ 0.0f, 0.0f };
};

} // namespace bb3d