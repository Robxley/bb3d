#pragma once

#if defined(BB3D_ENABLE_EDITOR)

#include "bb3d/editor/EditorPanel.hpp"
#include "bb3d/scene/Entity.hpp"
#include <string>
#include <vector>

namespace bb3d::editor {

/**
 * @brief Outliner panel displaying scene entities grouped by category with selection and creation tools.
 */
class SceneHierarchyPanel : public EditorPanel {
public:
    static constexpr std::string_view kId = "bb3d_scene_hierarchy";
    static constexpr std::string_view kTitle = "Hierarchy";

    void onImGuiRender() override;

    [[nodiscard]] std::string_view getId() const override { return kId; }
    [[nodiscard]] std::string_view getTitle() const override { return kTitle; }

    [[nodiscard]] const std::string& getFocusedComponent() const noexcept { return m_focusedComponent; }
    void setFocusedComponent(std::string_view comp) { m_focusedComponent = comp; }

private:
    void drawEntityNode(Entity entity);
    void drawComponentList(Entity entity);

    std::string m_focusedComponent;
    int m_entityCount = 0;
};

} // namespace bb3d::editor

#endif // BB3D_ENABLE_EDITOR
