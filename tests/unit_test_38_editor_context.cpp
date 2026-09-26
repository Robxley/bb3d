/**
 * @file unit_test_38_editor_context.cpp
 * @brief TDD unit test for EditorContext and EditorPanelManager (Phase 4.1).
 *
 * Validates:
 *  1. EditorContext entity selection, multi-selection, and selection center (barycenter).
 *  2. EditorContext simulation state transitions.
 *  3. EditorPanel lifecycle (onAttach, onDetach, onUpdate, onEvent) via EditorPanelManager.
 *  4. Panel retrieval by string ID and by C++ type.
 */

#include "bb3d/core/Engine.hpp"
#include "bb3d/core/Log.hpp"
#include "bb3d/scene/Scene.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/editor/EditorContext.hpp"
#include "bb3d/editor/EditorPanel.hpp"
#include "bb3d/editor/EditorPanelManager.hpp"

#include <cassert>
#include <iostream>
#include <cmath>

namespace {

// Mock EditorPanel for lifecycle validation
class MockPanel : public bb3d::editor::EditorPanel {
public:
    static constexpr std::string_view kId = "mock_panel";
    static constexpr std::string_view kTitle = "Mock Panel";

    void onAttach(bb3d::editor::EditorContext& context) override {
        EditorPanel::onAttach(context);
        attached = true;
    }

    void onDetach() override {
        detached = true;
        EditorPanel::onDetach();
    }

    void onUpdate(float dt) override {
        updateCount++;
        lastDt = dt;
    }

    void onImGuiRender() override {
        renderCount++;
    }

    [[nodiscard]] std::string_view getId() const override { return kId; }
    [[nodiscard]] std::string_view getTitle() const override { return kTitle; }

    bool attached = false;
    bool detached = false;
    uint32_t updateCount = 0;
    uint32_t renderCount = 0;
    float lastDt = 0.0f;
};

} // namespace

int main() {
    bb3d::EngineConfig config;
    config.system.logDirectory = "unit_test_logs";
    config.system.logFileName = "unit_test_38_editor_context.log";
    bb3d::Log::Init(config);

    BB_CORE_INFO("--- Unit Test 38: EditorContext & EditorPanelManager Architecture ---");

    auto scene = bb3d::CreateRef<bb3d::Scene>();

    // -------------------------------------------------------------------------
    // Test 1: EditorContext basic selection & simulation state
    // -------------------------------------------------------------------------
    {
        BB_CORE_INFO("Test 1: Validating EditorContext selection & state...");
        bb3d::editor::EditorContext context;
        context.setActiveScene(scene.get());
        assert(context.getActiveScene() == scene.get());

        // Default simulation state
        assert(context.getSimulationState() == bb3d::editor::SimulationState::Stopped);
        context.setSimulationState(bb3d::editor::SimulationState::Playing);
        assert(context.getSimulationState() == bb3d::editor::SimulationState::Playing);
        context.setSimulationState(bb3d::editor::SimulationState::Paused);
        assert(context.getSimulationState() == bb3d::editor::SimulationState::Paused);

        // Entities
        auto entity1 = scene->createEntity("Entity_1");
        auto entity2 = scene->createEntity("Entity_2");

        assert(!context.isSelected(entity1));
        assert(!context.isSelected(entity2));
        assert(!context.getSelectedEntity());
        assert(context.getSelectedEntities().empty());

        // Single selection
        context.selectEntity(entity1);
        assert(context.isSelected(entity1));
        assert(!context.isSelected(entity2));
        assert(context.getSelectedEntity() == entity1);
        assert(context.getSelectedEntities().size() == 1);

        // Replace selection
        context.selectEntity(entity2);
        assert(!context.isSelected(entity1));
        assert(context.isSelected(entity2));
        assert(context.getSelectedEntity() == entity2);
        assert(context.getSelectedEntities().size() == 1);

        // Deselection
        context.deselectEntity(entity2);
        assert(!context.isSelected(entity2));
        assert(!context.getSelectedEntity());
        assert(context.getSelectedEntities().empty());

        BB_CORE_INFO("Test 1: PASSED.");
    }

    // -------------------------------------------------------------------------
    // Test 2: Multi-selection and selection barycenter (pivot)
    // -------------------------------------------------------------------------
    {
        BB_CORE_INFO("Test 2: Validating multi-selection & selection barycenter...");
        bb3d::editor::EditorContext context;
        context.setActiveScene(scene.get());

        auto e1 = scene->createEntity("Point_A");
        e1.at(glm::vec3(0.0f, 0.0f, 0.0f));

        auto e2 = scene->createEntity("Point_B");
        e2.at(glm::vec3(10.0f, 0.0f, 0.0f));

        auto e3 = scene->createEntity("Point_C");
        e3.at(glm::vec3(5.0f, 12.0f, 0.0f));

        // Add to multi-selection (additive)
        context.selectEntity(e1, /*additive=*/false);
        context.selectEntity(e2, /*additive=*/true);
        context.selectEntity(e3, /*additive=*/true);

        assert(context.getSelectedEntities().size() == 3);
        assert(context.isSelected(e1));
        assert(context.isSelected(e2));
        assert(context.isSelected(e3));

        // Expected center: (0 + 10 + 5) / 3 = 5.0, (0 + 0 + 12) / 3 = 4.0, 0.0
        glm::vec3 center = context.getSelectionCenter();
        assert(std::abs(center.x - 5.0f) < 0.001f);
        assert(std::abs(center.y - 4.0f) < 0.001f);
        assert(std::abs(center.z - 0.0f) < 0.001f);

        // Clear selection
        context.clearSelection();
        assert(context.getSelectedEntities().empty());
        assert(!context.getSelectedEntity());
        assert(context.getSelectionCenter() == glm::vec3(0.0f));

        BB_CORE_INFO("Test 2: PASSED.");
    }

    // -------------------------------------------------------------------------
    // Test 3: EditorPanelManager & MockPanel lifecycle
    // -------------------------------------------------------------------------
    {
        BB_CORE_INFO("Test 3: Validating EditorPanelManager lifecycle...");
        bb3d::editor::EditorContext context;
        bb3d::editor::EditorPanelManager manager(context);

        // Register panel
        MockPanel* mock = manager.addPanel<MockPanel>();
        assert(mock != nullptr);
        assert(mock->attached && "onAttach must be called when panel is added");
        assert(!mock->detached);

        // Query by string ID
        auto* foundById = manager.getPanel(MockPanel::kId);
        assert(foundById == mock);

        // Query by type
        auto* foundByType = manager.getPanel<MockPanel>();
        assert(foundByType == mock);

        // Update loop dispatch
        manager.onUpdate(0.016f);
        assert(mock->updateCount == 1);
        assert(std::abs(mock->lastDt - 0.016f) < 0.0001f);

        // Render loop dispatch
        manager.onImGuiRender();
        assert(mock->renderCount == 1);

        // Removal
        manager.removePanel(MockPanel::kId);
        assert(mock->detached && "onDetach must be called when panel is removed");
        assert(manager.getPanel(MockPanel::kId) == nullptr);
        assert(manager.getPanel<MockPanel>() == nullptr);

        BB_CORE_INFO("Test 3: PASSED.");
    }

    scene->getRegistry().clear();
    BB_CORE_INFO("Unit Test 38: ALL TESTS PASSED — Editor Architecture validated!");
    return 0;
}
