#pragma once

#if defined(BB3D_ENABLE_EDITOR)

#include "bb3d/core/Base.hpp"
#include <string_view>

union SDL_Event;

namespace bb3d::editor {

class EditorContext;

/**
 * @brief Pure abstract interface for all modular editor panels.
 *
 * Implements the lifecycle: onAttach() -> onUpdate() / onImGuiRender() / onEvent() -> onDetach().
 * Decouples individual panel implementations from the main application layer.
 */
class EditorPanel {
public:
    virtual ~EditorPanel() = default;

    /** @brief Called when the panel is registered into EditorPanelManager. */
    virtual void onAttach(EditorContext& context) { m_context = &context; }

    /** @brief Called when the panel is unregistered or destroyed. */
    virtual void onDetach() { m_context = nullptr; }

    /** @brief Per-frame logic update (dt in seconds). */
    virtual void onUpdate([[maybe_unused]] float deltaTime) {}

    /** @brief Primary ImGui render callback. */
    virtual void onImGuiRender() = 0;

    /** @brief Processes native SDL3 events. */
    virtual void onEvent([[maybe_unused]] const SDL_Event& event) {}

    /** @brief Unique immutable identifier string for the panel (e.g., "bb3d_scene_hierarchy"). */
    [[nodiscard]] virtual std::string_view getId() const = 0;

    /** @brief Human-readable title displayed in the ImGui tab/window header. */
    [[nodiscard]] virtual std::string_view getTitle() const = 0;

    /** @brief Optional FontAwesome or UTF-8 icon glyph string. */
    [[nodiscard]] virtual const char* getIcon() const { return ""; }

    /** @brief Panel open/visibility state. */
    [[nodiscard]] bool& isOpen() noexcept { return m_isOpen; }
    [[nodiscard]] bool isOpen() const noexcept { return m_isOpen; }
    void setOpen(bool open) noexcept { m_isOpen = open; }

protected:
    EditorContext* m_context = nullptr;
    bool m_isOpen = true;
};

} // namespace bb3d::editor

#endif // BB3D_ENABLE_EDITOR
