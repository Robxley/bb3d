#pragma once

#if defined(BB3D_ENABLE_EDITOR)

#include "bb3d/editor/EditorPanel.hpp"

namespace bb3d::editor {

/**
 * @brief Compact toolbar panel with simulation controls (Play, Pause, Reset) and utility actions.
 */
class ToolbarPanel : public EditorPanel {
public:
    static constexpr std::string_view kId = "bb3d_toolbar";
    static constexpr std::string_view kTitle = "Toolbar";

    void onImGuiRender() override;

    [[nodiscard]] std::string_view getId() const override { return kId; }
    [[nodiscard]] std::string_view getTitle() const override { return kTitle; }
};

} // namespace bb3d::editor

#endif // BB3D_ENABLE_EDITOR
