#include "bb3d/editor/EditorStyle.hpp"

#if defined(BB3D_ENABLE_EDITOR)

#include <imgui_internal.h>
#include <string>

namespace bb3d::editor {

void EditorStyle::ApplyDarkProTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // =========================================================================
    // 1. MÉTRIQUES, ARRONDIS & PADDINGS (Ergonomie DCC Moderne)
    // =========================================================================
    style.WindowRounding    = 4.0f;
    style.ChildRounding     = 3.0f;
    style.FrameRounding     = 4.0f;
    style.PopupRounding     = 5.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding      = 3.0f;
    style.TabRounding       = 4.0f;

    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.TabBorderSize     = 0.0f;

    style.WindowPadding     = ImVec2(10.0f, 10.0f);
    style.FramePadding      = ImVec2(8.0f, 5.0f);
    style.ItemSpacing       = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing  = ImVec2(6.0f, 4.0f);
    style.CellPadding       = ImVec2(6.0f, 4.0f);
    style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
    style.IndentSpacing     = 18.0f;
    style.ScrollbarSize     = 11.0f;
    style.GrabMinSize       = 10.0f;

    style.WindowTitleAlign         = ImVec2(0.0f, 0.5f);
    style.WindowMenuButtonPosition = ImGuiDir_None;
    style.ColorButtonPosition      = ImGuiDir_Right;

    // =========================================================================
    // 2. PALETTE DE COULEURS "DARK PRO" (Inspirée Slate UE5 & Blender 4)
    // =========================================================================
    
    // --- Textes & Visibilité ---
    colors[ImGuiCol_Text]                  = ImVec4(0.92f, 0.93f, 0.95f, 1.00f); // Blanc cassé doux
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.48f, 0.52f, 0.58f, 1.00f); // Gris moyen pour inactif

    // --- Arrière-plans de Fenêtres & Surfaces ---
    colors[ImGuiCol_WindowBg]              = ImVec4(0.10f, 0.11f, 0.13f, 1.00f); // #1A1D21 (Layer 1)
    colors[ImGuiCol_ChildBg]               = ImVec4(0.08f, 0.09f, 0.11f, 0.70f); // #15171B (Layer 2)
    colors[ImGuiCol_PopupBg]               = ImVec4(0.12f, 0.13f, 0.16f, 0.98f); // Popups opaques
    colors[ImGuiCol_Border]                = ImVec4(0.17f, 0.19f, 0.22f, 0.80f); // Bordures très subtiles
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    // --- Frames (Champs d'entrées, Sliders, Checkboxes) ---
    colors[ImGuiCol_FrameBg]               = ImVec4(0.14f, 0.16f, 0.19f, 1.00f); // #242930 (Layer 3)
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.20f, 0.23f, 0.27f, 1.00f); // #333B45 (Layer 4)
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.24f, 0.28f, 0.33f, 1.00f); // #3D4754 (Layer 5)

    // --- Barres de Titre & Menus ---
    colors[ImGuiCol_TitleBg]               = ImVec4(0.09f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.11f, 0.12f, 0.15f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.08f, 0.09f, 0.11f, 0.75f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.08f, 0.09f, 0.11f, 1.00f);

    // --- Barres de Défilement (Scrollbars) ---
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.08f, 0.09f, 0.11f, 0.50f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.22f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.30f, 0.34f, 0.40f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.38f, 0.43f, 0.50f, 1.00f);

    // --- Checkmarks & Curseurs (Grabs) ---
    colors[ImGuiCol_CheckMark]             = ImVec4(0.26f, 0.59f, 0.98f, 1.00f); // Steel Blue
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.26f, 0.53f, 0.96f, 0.85f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.33f, 0.62f, 1.00f, 1.00f);

    // --- Boutons Généraux ---
    colors[ImGuiCol_Button]                = ImVec4(0.18f, 0.20f, 0.24f, 1.00f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.24f, 0.28f, 0.34f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.16f, 0.38f, 0.75f, 1.00f); // Accent bleu au clic

    // --- En-têtes (CollapsingHeader, TreeNodes) ---
    colors[ImGuiCol_Header]                = ImVec4(0.16f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.22f, 0.26f, 0.32f, 1.00f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.18f, 0.36f, 0.68f, 1.00f);

    // --- Séparateurs & Poignées de Redimensionnement ---
    colors[ImGuiCol_Separator]             = ImVec4(0.16f, 0.18f, 0.22f, 0.80f);
    colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.26f, 0.59f, 0.98f, 0.78f);
    colors[ImGuiCol_SeparatorActive]       = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.20f, 0.23f, 0.27f, 0.30f);
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);

    // --- Onglets de Docking (Tabs) ---
    colors[ImGuiCol_Tab]                   = ImVec4(0.11f, 0.13f, 0.15f, 1.00f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.24f, 0.28f, 0.35f, 1.00f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.16f, 0.19f, 0.23f, 1.00f);
    colors[ImGuiCol_TabUnfocused]          = ImVec4(0.09f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.13f, 0.15f, 0.18f, 1.00f);

    // --- Docking & Aperçu de Drag ---
    colors[ImGuiCol_DockingPreview]        = ImVec4(0.18f, 0.42f, 0.88f, 0.35f);
    colors[ImGuiCol_DockingEmptyBg]        = ImVec4(0.07f, 0.08f, 0.09f, 1.00f);

    // --- Tables & Lignes Alternées ---
    colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.13f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.18f, 0.21f, 0.25f, 1.00f);
    colors[ImGuiCol_TableBorderLight]      = ImVec4(0.14f, 0.16f, 0.19f, 0.70f);
    colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.00f, 1.00f, 1.00f, 0.02f);

    // --- Navigation, Focus & DragDrop ---
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.18f, 0.42f, 0.88f, 0.35f);
    colors[ImGuiCol_DragDropTarget]        = ImVec4(0.20f, 0.55f, 1.00f, 0.95f);
    colors[ImGuiCol_NavHighlight]          = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.00f, 0.00f, 0.00f, 0.40f);
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);
}

bool EditorStyle::DrawVec3Control(std::string_view label, glm::vec3& values, float resetValue, float columnWidth) {
    bool changed = false;

    ImGui::PushID(label.data());

    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnWidth(0, columnWidth);
    ImGui::TextUnformatted(label.data());
    ImGui::NextColumn();

    ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 0.0f));

    float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
    ImVec2 buttonSize = { lineHeight + 2.0f, lineHeight };

    // --- AXE X (Rouge Corail) ---
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.75f, 0.22f, 0.22f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.88f, 0.28f, 0.28f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.65f, 0.18f, 0.18f, 1.00f));
    if (ImGui::Button("X", buttonSize)) {
        values.x = resetValue;
        changed = true;
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    changed |= ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
    ImGui::PopItemWidth();
    ImGui::SameLine();

    // --- AXE Y (Vert Émeraude) ---
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.22f, 0.65f, 0.22f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.78f, 0.28f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.18f, 0.55f, 0.18f, 1.00f));
    if (ImGui::Button("Y", buttonSize)) {
        values.y = resetValue;
        changed = true;
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    changed |= ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
    ImGui::PopItemWidth();
    ImGui::SameLine();

    // --- AXE Z (Bleu Cobalt) ---
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.44f, 0.80f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.54f, 0.95f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.16f, 0.36f, 0.70f, 1.00f));
    if (ImGui::Button("Z", buttonSize)) {
        values.z = resetValue;
        changed = true;
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    changed |= ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");
    ImGui::PopItemWidth();

    ImGui::PopStyleVar();
    ImGui::Columns(1);
    ImGui::PopID();

    return changed;
}

} // namespace bb3d::editor

#endif // BB3D_ENABLE_EDITOR
