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
    InputManager();
    ~InputManager() = default;

    /** @brief Met à jour l'état interne (reset deltas). */
    void update(); 

    /** @brief Réinitialise les deltas (scroll, etc.) au début d'une frame. */
    void clearDeltas();

    /** @brief Traite un événement SDL entrant. */
    void onEvent(const SDL_Event& event);

    /** @name Polling Bas Niveau
     * @{
     */
    [[nodiscard]] bool isKeyPressed(Key key) const;
    [[nodiscard]] bool isKeyJustPressed(Key key) const;
    [[nodiscard]] bool isKeyJustReleased(Key key) const;

    [[nodiscard]] bool isMouseButtonPressed(Mouse button) const;
    [[nodiscard]] bool isMouseButtonJustPressed(Mouse button) const;
    [[nodiscard]] bool isMouseButtonJustReleased(Mouse button) const;

    [[nodiscard]] glm::vec2 getMousePosition() const { return m_currentMousePos; }
    [[nodiscard]] glm::vec2 getMouseDelta() const { return m_mouseDelta; }
    [[nodiscard]] glm::vec2 getMouseScroll() const { return m_mouseScroll; }
    /** @} */

    /** @name Mapping Haut Niveau
     * @{
     */
    /**
     * @brief Associe une action logique à une touche physique.
     * @param name Nom de l'action (ex: "Jump", "Fire").
     * @param key Touche clavier à associer.
     */
    void mapAction(std::string_view name, Key key);

    /**
     * @brief Associe une action logique à un bouton de souris.
     * @param name Nom de l'action.
     * @param button Bouton souris à associer.
     */
    void mapAction(std::string_view name, Mouse button);

    /**
     * @brief Associe un axe logique (valeur float entre -1 et 1) à une paire de touches.
     * @param name Nom de l'axe (ex: "MoveForward").
     * @param positive Touche pour la valeur +1.0 (ex: Key::W).
     * @param negative Touche pour la valeur -1.0 (ex: Key::S).
     */
    void mapAxis(std::string_view name, Key positive, Key negative);
    /** @} */

    /** @name Requêtes d'Actions
     * @{
     */
    /**
     * @brief Vérifie si une action mappée est active.
     * @param name Nom de l'action.
     * @return true si l'input associé est pressé, false sinon.
     */
    [[nodiscard]] bool isActionPressed(std::string_view name) const;
    [[nodiscard]] bool isActionJustPressed(std::string_view name) const;
    [[nodiscard]] bool isActionJustReleased(std::string_view name) const;

    /**
     * @brief Récupère la valeur d'un axe virtuel.
     * @param name Nom de l'axe.
     * @return float Valeur résultante (1.0, -1.0 ou 0.0 si aucune/les deux touches sont pressées).
     */
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
    glm::vec2 m_mouseDelta{0.0f};
    glm::vec2 m_mouseScroll{0.0f};
};

} // namespace bb3d
