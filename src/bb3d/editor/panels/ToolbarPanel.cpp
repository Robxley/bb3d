#include "bb3d/editor/panels/ToolbarPanel.hpp"

#if defined(BB3D_ENABLE_EDITOR)

#include "bb3d/editor/EditorContext.hpp"
#include "bb3d/core/Engine.hpp"
#include "bb3d/render/Renderer.hpp"
#include "bb3d/core/IconsFontAwesome6.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace bb3d::editor {

void ToolbarPanel::onImGuiRender() {
    if (!m_context) return;
    Engine* engine = m_context->getEngine();
    if (!engine) return;

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 2.0f));

    if (ImGui::Begin("Toolbar", &m_isOpen, flags)) {
        ImGui::PopStyleVar();

        bool paused = engine->isPhysicsPaused();

        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float playPauseWidth = ImGui::CalcTextSize(paused ? ICON_FA_PLAY " Play" : ICON_FA_PAUSE " Pause").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        float resetWidth = ImGui::CalcTextSize(ICON_FA_ARROW_ROTATE_LEFT " Reset Scene").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        float screenshotWidth = ImGui::CalcTextSize(ICON_FA_CAMERA " Screenshot").x + ImGui::GetStyle().FramePadding.x * 2.0f;

        float totalWidth = playPauseWidth + spacing + resetWidth + spacing + 2.0f + spacing + screenshotWidth;

        float off = (ImGui::GetContentRegionAvail().x - totalWidth) * 0.5f;
        if (off > 0.0f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);

        if (paused) {
            if (ImGui::Button(ICON_FA_PLAY " Play")) {
                m_context->setSimulationState(SimulationState::Playing);
            }
        } else {
            if (ImGui::Button(ICON_FA_PAUSE " Pause")) {
                m_context->setSimulationState(SimulationState::Paused);
            }
        }

        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_ARROW_ROTATE_LEFT " Reset Scene")) {
            m_context->setSimulationState(SimulationState::Stopped);
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        if (ImGui::Button(ICON_FA_CAMERA " Screenshot")) {
            auto now = std::chrono::system_clock::now();
            auto in_time_t = std::chrono::system_clock::to_time_t(now);
            std::tm tm_buf{};
#if defined(_MSC_VER)
            localtime_s(&tm_buf, &in_time_t);
#else
            localtime_r(&in_time_t, &tm_buf);
#endif
            std::stringstream ss;
            ss << "screenshots/screenshot_" << std::put_time(&tm_buf, "%Y%m%d_%H%M%S") << ".png";
            engine->renderer().saveScreenshot(ss.str());
        }
    } else {
        ImGui::PopStyleVar();
    }
    ImGui::End();
}

} // namespace bb3d::editor

#endif // BB3D_ENABLE_EDITOR
