#include "bb3d/editor/panels/SceneSettingsPanel.hpp"

#if defined(BB3D_ENABLE_EDITOR)

#include "bb3d/editor/EditorContext.hpp"
#include "bb3d/scene/Scene.hpp"
#include "bb3d/core/Engine.hpp"
#include "bb3d/render/Renderer.hpp"
#include "bb3d/core/PickingSystem.hpp"
#include "bb3d/core/IconsFontAwesome6.h"

#include <imgui.h>

namespace bb3d::editor {

void SceneSettingsPanel::onImGuiRender() {
    if (!m_context) return;
    Scene* scene = m_context->getActiveScene();
    if (!scene) return;

    ImGui::Begin(ICON_FA_SLIDERS " Scene Settings", &m_isOpen);

    if (ImGui::CollapsingHeader(ICON_FA_CLOUD " Environment", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& fog = scene->getFog();
        FogSettings newFog = fog;

        const char* fogTypes[] = { "None", "Linear", "Exponential", "Exponential Squared" };
        int currentFog = static_cast<int>(fog.type);
        if (ImGui::Combo("Fog Type", &currentFog, fogTypes, IM_ARRAYSIZE(fogTypes))) {
            newFog.type = static_cast<FogType>(currentFog);
        }

        if (newFog.type != FogType::None) {
            ImGui::ColorEdit3("Fog Color", &newFog.color.x);
            if (newFog.type == FogType::Linear) {
                ImGui::DragFloat("Fog Start", &newFog.start, 1.0f, 0.0f, 1000.0f);
                ImGui::DragFloat("Fog End", &newFog.end, 1.0f, 0.0f, 2000.0f);
            } else {
                ImGui::DragFloat("Fog Density", &newFog.density, 0.001f, 0.0f, 1.0f);
            }
        }

        if (currentFog != static_cast<int>(fog.type) ||
            newFog.color != fog.color ||
            newFog.density != fog.density ||
            newFog.start != fog.start ||
            newFog.end != fog.end) {
            scene->setFog(newFog);
        }
    }

    ImGui::Separator();
    ImGui::Checkbox("Show Grid", &m_showGrid);

    if (ImGui::Checkbox("Debug Physics Colliders", &m_showPhysicsColliders)) {
        if (scene->getEngineContext()) {
            scene->getEngineContext()->renderer().setDebugPhysicsColliders(m_showPhysicsColliders);
        }
    }

    if (scene->getEngineContext()) {
        auto& config = const_cast<EngineConfig&>(scene->getEngineContext()->GetConfig());
        auto& renderer = scene->getEngineContext()->renderer();

        bool shadowsEnabled = renderer.isShadowsEnabled();
        if (ImGui::Checkbox("Enable Cascaded Shadows", &shadowsEnabled)) {
            renderer.setShadowsEnabled(shadowsEnabled);
        }

        if (shadowsEnabled && ImGui::TreeNodeEx("Shadow Biases (Advanced)", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextWrapped("Adjust these values to fix 'Peter Panning' or 'Shadow Acne'.");

            float normalBias = config.graphics.shadowNormalBias;
            float shaderDepthBias = config.graphics.shadowShaderDepthBias;

            if (ImGui::DragFloat("Normal Bias", &normalBias, 0.001f, 0.0f, 0.5f, "%.4f")) {
                config.graphics.shadowNormalBias = normalBias;
            }
            if (ImGui::DragFloat("Shader Depth Bias", &shaderDepthBias, 0.0001f, 0.0f, 0.01f, "%.5f")) {
                config.graphics.shadowShaderDepthBias = shaderDepthBias;
            }

            float constantBias = config.graphics.shadowDepthBiasConstant;
            float slopeBias = config.graphics.shadowDepthBiasSlope;

            if (ImGui::DragFloat("Pipeline Constant Bias", &constantBias, 0.1f, 0.0f, 10.0f, "%.1f")) {
                config.graphics.shadowDepthBiasConstant = constantBias;
            }
            if (ImGui::DragFloat("Pipeline Slope Bias", &slopeBias, 0.1f, 0.0f, 10.0f, "%.1f")) {
                config.graphics.shadowDepthBiasSlope = slopeBias;
            }

            ImGui::TreePop();
        }
    }

    ImGui::Separator();
    if (ImGui::CollapsingHeader(ICON_FA_ARROW_POINTER " Picking & Selection", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* pickingSys = scene->getEngineContext() ? scene->getEngineContext()->GetPickingSystem() : nullptr;
        if (pickingSys) {
            const char* pickingModes[] = { "None", "Physics Raycast", "Color Picking (GPU)" };
            int currentMode = static_cast<int>(pickingSys->getMode());
            if (ImGui::Combo("Picking Mode", &currentMode, pickingModes, IM_ARRAYSIZE(pickingModes))) {
                pickingSys->setMode(static_cast<PickingMode>(currentMode));
            }
            ImGui::TextWrapped("Physics Raycast works on entities with PhysicsComponent. Color Picking works on any mesh.");
        } else {
            ImGui::TextDisabled("Picking System not initialized in Engine.");
        }
    }

    ImGui::End();
}

} // namespace bb3d::editor

#endif // BB3D_ENABLE_EDITOR
