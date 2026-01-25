#pragma once

#include "bb3d/input/InputCodes.hpp"
#include <string>
#include <string_view>
#include <unordered_map>
#include <glm/vec2.hpp>

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
 * 
 * Permet d'abstraire les entrées physiques (Clavier/Souris) en actions logiques (ex: "Jump").
 */
class InputManager {
public:
    InputManager();
    ~InputManager() = default;

    /** @brief Met à jour l'état interne (appelé par Engine). */
    void update(); 

    /** @name Polling Bas Niveau
     * @{
     */
    [[nodiscard]] bool isKeyPressed(Key key) const;
    [[nodiscard]] bool isKeyJustPressed(Key key) const;
    [[nodiscard]] bool isKeyJustReleased(Key key) const;

    [[nodiscard]] bool isMouseButtonPressed(Mouse button) const;
    [[nodiscard]] bool isMouseButtonJustPressed(Mouse button) const;
    [[nodiscard]] bool isMouseButtonJustReleased(Mouse button) const;

    [[nodiscard]] glm::vec2 getMousePosition() const;
    [[nodiscard]] glm::vec2 getMouseDelta() const;
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
    [[nodiscard]] bool isActionJustPressed(std::string_view name) const;
    [[nodiscard]] bool isActionJustReleased(std::string_view name) const;
    [[nodiscard]] float getAxis(std::string_view name) const;
    /** @} */

private:
    std::unordered_map<std::string, InputBinding> m_actions;
    std::unordered_map<std::string, AxisBinding> m_axes;

    // États du clavier
    uint8_t m_currentKeyState[512]; // SDL_SCANCODE_COUNT est généralement < 512
    uint8_t m_previousKeyState[512];

    // États de la souris
    uint32_t m_currentMouseState = 0;
    uint32_t m_previousMouseState = 0;
    glm::vec2 m_currentMousePos{0.0f};
    glm::vec2 m_previousMousePos{0.0f};
};

} // namespace bb3d