#include "bb3d/editor/panels/ConsolePanel.hpp"

#if defined(BB3D_ENABLE_EDITOR)

#include "bb3d/core/IconsFontAwesome6.h"
#include <imgui.h>
#include <imgui_internal.h>

namespace bb3d::editor {

ConsolePanel::ConsolePanel() {
    m_messages.reserve(1024);
}

void ConsolePanel::addMessage(ConsoleMessage::Level level, std::string_view text) {
    if (m_messages.size() >= 2048) {
        m_messages.erase(m_messages.begin(), m_messages.begin() + 256);
    }
    m_messages.push_back({ level, std::string(text) });
}

void ConsolePanel::clear() {
    m_messages.clear();
}

void ConsolePanel::onImGuiRender() {
    ImGui::Begin(ICON_FA_TERMINAL " Console", &m_isOpen);

    // Toolbar filters
    if (ImGui::Button("Clear")) {
        clear();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-Scroll", &m_autoScroll);
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();
    ImGui::Checkbox("Trace", &m_filterTrace);
    ImGui::SameLine();
    ImGui::Checkbox("Info", &m_filterInfo);
    ImGui::SameLine();
    ImGui::Checkbox("Warn", &m_filterWarn);
    ImGui::SameLine();
    ImGui::Checkbox("Error", &m_filterError);

    ImGui::Separator();

    ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    for (const auto& msg : m_messages) {
        if (msg.level == ConsoleMessage::Level::Trace && !m_filterTrace) continue;
        if (msg.level == ConsoleMessage::Level::Info  && !m_filterInfo)  continue;
        if (msg.level == ConsoleMessage::Level::Warn  && !m_filterWarn)  continue;
        if (msg.level == ConsoleMessage::Level::Error && !m_filterError) continue;

        ImVec4 color = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
        if (msg.level == ConsoleMessage::Level::Trace) color = ImVec4(0.5f, 0.5f, 0.6f, 1.0f);
        else if (msg.level == ConsoleMessage::Level::Info) color = ImVec4(0.7f, 0.9f, 1.0f, 1.0f);
        else if (msg.level == ConsoleMessage::Level::Warn) color = ImVec4(1.0f, 0.8f, 0.3f, 1.0f);
        else if (msg.level == ConsoleMessage::Level::Error) color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(msg.text.c_str());
        ImGui::PopStyleColor();
    }

    if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
    ImGui::End();
}

} // namespace bb3d::editor

#endif // BB3D_ENABLE_EDITOR
