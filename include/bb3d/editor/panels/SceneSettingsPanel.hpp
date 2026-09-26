#pragma once

#if defined(BB3D_ENABLE_EDITOR)

#include "bb3d/editor/EditorPanel.hpp"

namespace bb3d::editor {

/**
 * @brief Scene and renderer environment settings panel (Fog, Cascaded Shadows, Debug Visualizers).
 */
class SceneSettingsPanel : public EditorPanel {
public:
    static constexpr std::string_view kId = "bb3d_scene_settings";
    static constexpr std::string_view kTitle = "Scene Settings";

    void onImGuiRender() override;

    [[nodiscard]] std::string_view getId() const override { return kId; }
    [[nodiscard]] std::string_view getTitle() const override { return kTitle; }

private:
    bool m_showGrid = true;
    bool m_showPhysicsColliders = false;
};

} // namespace bb3d::editor

#endif // BB3D_ENABLE_EDITOR
