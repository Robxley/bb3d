#pragma once

#if defined(BB3D_ENABLE_EDITOR)

#include "bb3d/editor/EditorPanel.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/render/Model.hpp"
#include <string>
#include <functional>
#include <glm/vec4.hpp>

#include <imgui.h>

namespace bb3d {
class Material;
}

namespace bb3d::editor {

/**
 * @brief Property inspector panel displaying and editing components of the selected entity.
 *
 * Implements isolated component properties, ergonomic Vector3 controls,
 * and fixes static mutable variables N5, N6, and N7.
 */
class InspectorPanel : public EditorPanel {
public:
    static constexpr std::string_view kId = "bb3d_inspector";
    static constexpr std::string_view kTitle = "Inspector";

    void onImGuiRender() override;

    [[nodiscard]] std::string_view getId() const override { return kId; }
    [[nodiscard]] std::string_view getTitle() const override { return kTitle; }

    [[nodiscard]] const std::string& getFocusedComponent() const noexcept { return m_focusedComponent; }
    void setFocusedComponent(std::string_view comp) { m_focusedComponent = comp; }

private:
    void drawComponentList(Entity entity);
    void drawMaterialUI(Material* material);
    bool drawComponentHeader(const char* name, const char* icon, const ImVec4& color,
                             bool canRemove = true, std::function<void()> onRemove = nullptr);

    std::string m_focusedComponent;

    // Member states replacing static mutables (Fixes N6 & N7)
    glm::vec4 m_particleTint{ 1.0f, 1.0f, 1.0f, 1.0f };
    ModelLoadConfig m_modelLoadConfig;
};

} // namespace bb3d::editor

#endif // BB3D_ENABLE_EDITOR
