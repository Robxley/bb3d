#pragma once

#if defined(BB3D_ENABLE_EDITOR)

#include "bb3d/core/Base.hpp"
#include <string_view>
#include <glm/vec3.hpp>
#include <imgui.h>

namespace bb3d::editor {

/**
 * @brief High-performance Dark Pro Design System for bb3d editor.
 *
 * Implements the Slate & Dark Charcoal palette, elevation layers,
 * ergonomic padding, and composite controls based on Unreal Engine 5 & Blender 4.
 */
class EditorStyle {
public:
    /** @brief Applies the full Dark Pro theme metrics, colors, and rounding to ImGui. */
    static void ApplyDarkProTheme();

    /**
     * @brief Renders an ergonomic 3-axis DragFloat composite control with X (Red), Y (Green), Z (Blue) reset buttons.
     * @param label Property label shown on the left column.
     * @param values Vector3 to modify in-place.
     * @param resetValue Value restored when clicking the X/Y/Z button (default 0.0f).
     * @param columnWidth Width reserved for the label column (default 100.0f).
     * @return True if any component value changed.
     */
    static bool DrawVec3Control(std::string_view label, glm::vec3& values, float resetValue = 0.0f, float columnWidth = 100.0f);
};

} // namespace bb3d::editor

#endif // BB3D_ENABLE_EDITOR
