#include "bb3d/editor/panels/ViewportPanel.hpp"

#if defined(BB3D_ENABLE_EDITOR)

#include "bb3d/editor/EditorContext.hpp"
#include "bb3d/render/RenderTarget.hpp"
#include "bb3d/scene/Scene.hpp"
#include "bb3d/core/IconsFontAwesome6.h"

namespace bb3d::editor {

void ViewportPanel::onImGuiRender() {
    if (!m_context) return;
    Scene* scene = m_context->getActiveScene();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });
    ImGui::Begin(ICON_FA_IMAGE " Viewport", &m_isOpen);

    if (m_renderTarget && m_textureId) {
        ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();

        uint32_t width = static_cast<uint32_t>(viewportPanelSize.x);
        uint32_t height = static_cast<uint32_t>(viewportPanelSize.y);

        if (width > 0 && height > 0 && (width != m_viewportSize.x || height != m_viewportSize.y)) {
            if (width != m_renderTarget->getExtent().width || height != m_renderTarget->getExtent().height) {
                m_viewportSize = { width, height };
                m_viewportSizeChanged = true;
            } else {
                m_viewportSize = { width, height };
                m_viewportSizeChanged = false;
            }
        }

        m_isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        m_isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

        ImVec2 viewportContentPos = ImGui::GetCursorScreenPos();
        ImGui::Image(m_textureId, viewportPanelSize);

        // Mouse Picking in Viewport
        if (scene && m_isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemActive()) {
            ImVec2 mousePos = ImGui::GetMousePos();
            float uvX = (mousePos.x - viewportContentPos.x) / viewportPanelSize.x;
            float uvY = (mousePos.y - viewportContentPos.y) / viewportPanelSize.y;

            if (uvX >= 0.0f && uvX <= 1.0f && uvY >= 0.0f && uvY <= 1.0f) {
                Entity picked = scene->pickEntity({ uvX, uvY });
                if (picked) {
                    bool ctrl = ImGui::GetIO().KeyCtrl;
                    m_context->selectEntity(picked, ctrl);
                } else {
                    m_context->clearSelection();
                }
            }
        }
    } else {
        ImGui::Text("No RenderTarget available (Offscreen Rendering disabled?)");
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace bb3d::editor

#endif // BB3D_ENABLE_EDITOR
