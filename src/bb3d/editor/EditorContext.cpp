#include "bb3d/editor/EditorContext.hpp"

#if defined(BB3D_ENABLE_EDITOR)

#include "bb3d/scene/Scene.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/core/Engine.hpp"
#include "bb3d/core/Log.hpp"
#include <algorithm>

namespace bb3d::editor {

EditorContext::EditorContext() {
    m_selectedEntities.reserve(32);
}

void EditorContext::selectEntity(Entity entity, bool additive) {
    if (!entity) {
        if (!additive) clearSelection();
        return;
    }

    if (!additive) {
        m_selectedEntities.clear();
        m_selectedEntities.push_back(entity);
    } else {
        auto it = std::find(m_selectedEntities.begin(), m_selectedEntities.end(), entity);
        if (it == m_selectedEntities.end()) {
            m_selectedEntities.push_back(entity);
        }
    }
}

void EditorContext::deselectEntity(Entity entity) {
    auto it = std::find(m_selectedEntities.begin(), m_selectedEntities.end(), entity);
    if (it != m_selectedEntities.end()) {
        m_selectedEntities.erase(it);
    }
}

void EditorContext::clearSelection() {
    m_selectedEntities.clear();
}

Entity EditorContext::getSelectedEntity() const {
    if (m_selectedEntities.empty()) return {};
    return m_selectedEntities.back();
}

bool EditorContext::isSelected(Entity entity) const {
    if (!entity || m_selectedEntities.empty()) return false;
    return std::find(m_selectedEntities.begin(), m_selectedEntities.end(), entity) != m_selectedEntities.end();
}

glm::vec3 EditorContext::getSelectionCenter() const {
    if (m_selectedEntities.empty()) return glm::vec3(0.0f);

    glm::vec3 sum(0.0f);
    uint32_t count = 0;

    for (const auto& entity : m_selectedEntities) {
        if (entity && entity.has<TransformComponent>()) {
            sum += entity.get<TransformComponent>().translation;
            count++;
        }
    }

    if (count == 0) return glm::vec3(0.0f);
    return sum / static_cast<float>(count);
}

void EditorContext::setSimulationState(SimulationState state) {
    if (m_simulationState == state) return;

    m_simulationState = state;
    BB_CORE_INFO("EditorContext: Simulation state changed to {}",
                 state == SimulationState::Playing ? "Playing" :
                 state == SimulationState::Paused  ? "Paused"  : "Stopped");

    if (m_engine) {
        if (state == SimulationState::Playing) {
            m_engine->setPhysicsPaused(false);
        } else if (state == SimulationState::Paused) {
            m_engine->setPhysicsPaused(true);
        } else if (state == SimulationState::Stopped) {
            m_engine->setPhysicsPaused(true);
            m_engine->resetScene();
        }
    }
}

} // namespace bb3d::editor

#endif // BB3D_ENABLE_EDITOR
