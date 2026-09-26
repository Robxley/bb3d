#pragma once

#if defined(BB3D_ENABLE_EDITOR)

#include "bb3d/editor/EditorPanel.hpp"
#include <glm/vec2.hpp>
#include <imgui.h>

namespace bb3d {
class RenderTarget;
class Scene;
}

namespace bb3d::editor {

/**
 * @brief Viewport panel displaying the offscreen rendered 3D scene texture.
 *
 * Handles ImGui window resizing, mouse picking UV coordinates, and input focus state.
 */
class ViewportPanel : public EditorPanel {
public:
    static constexpr std::string_view kId = "bb3d_viewport";
    static constexpr std::string_view kTitle = "Viewport";

    void onImGuiRender() override;

    [[nodiscard]] std::string_view getId() const override { return kId; }
    [[nodiscard]] std::string_view getTitle() const override { return kTitle; }

    [[nodiscard]] glm::uvec2 getViewportSize() const noexcept { return m_viewportSize; }
    [[nodiscard]] bool hasViewportSizeChanged() const noexcept { return m_viewportSizeChanged; }
    void clearViewportSizeChanged() noexcept { m_viewportSizeChanged = false; }

    [[nodiscard]] bool isFocused() const noexcept { return m_isFocused; }
    [[nodiscard]] bool isHovered() const noexcept { return m_isHovered; }

    void setRenderTarget(RenderTarget* rt) noexcept { m_renderTarget = rt; }
    void setTextureId(ImTextureID texID) noexcept { m_textureId = texID; }

private:
    RenderTarget* m_renderTarget = nullptr;
    ImTextureID m_textureId = 0;

    glm::uvec2 m_viewportSize{ 0, 0 };
    bool m_viewportSizeChanged = false;

    bool m_isFocused = false;
    bool m_isHovered = false;
};

} // namespace bb3d::editor

#endif // BB3D_ENABLE_EDITOR
