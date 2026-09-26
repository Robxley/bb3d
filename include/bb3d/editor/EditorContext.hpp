#pragma once

#if defined(BB3D_ENABLE_EDITOR)

#include "bb3d/core/Base.hpp"
#include "bb3d/scene/Entity.hpp"
#include <vector>
#include <glm/vec3.hpp>

namespace bb3d {
class Scene;
class Engine;
} // namespace bb3d

namespace bb3d::editor {

/**
 * @brief Playback simulation state of the engine when in editor mode.
 */
enum class SimulationState {
    Stopped,
    Playing,
    Paused
};

/**
 * @brief Central state bus for editor panels.
 *
 * Encapsulates the active scene, entity selection (single and multi),
 * hovered entity, pivot center, and simulation controls.
 * Eliminates global statics and tight panel-to-panel coupling.
 */
class EditorContext {
public:
    EditorContext();
    ~EditorContext() = default;

    // --- Active Scene Management ---
    void setActiveScene(Scene* scene) noexcept { m_activeScene = scene; }
    [[nodiscard]] Scene* getActiveScene() const noexcept { return m_activeScene; }

    // --- Entity Selection ---
    /**
     * @brief Selects an entity.
     * @param entity The entity to select.
     * @param additive If true, adds to current multi-selection; if false, replaces selection.
     */
    void selectEntity(Entity entity, bool additive = false);

    /** @brief Removes an entity from the active selection. */
    void deselectEntity(Entity entity);

    /** @brief Deselects all currently selected entities. */
    void clearSelection();

    /** @brief Returns the primary selected entity (or null if empty). */
    [[nodiscard]] Entity getSelectedEntity() const;

    /** @brief Returns all currently selected entities in multi-selection. */
    [[nodiscard]] const std::vector<Entity>& getSelectedEntities() const noexcept { return m_selectedEntities; }

    /** @brief Checks if a specific entity is currently selected. */
    [[nodiscard]] bool isSelected(Entity entity) const;

    // --- Hovered Entity ---
    void setHoveredEntity(Entity entity) noexcept { m_hoveredEntity = entity; }
    [[nodiscard]] Entity getHoveredEntity() const noexcept { return m_hoveredEntity; }

    // --- Selection Pivot / Center ---
    /**
     * @brief Calculates the world-space center of mass (barycenter) of all selected entities.
     * Returns vec3(0) if no entities with TransformComponent are selected.
     */
    [[nodiscard]] glm::vec3 getSelectionCenter() const;

    // --- Simulation Controls ---
    void setSimulationState(SimulationState state);
    [[nodiscard]] SimulationState getSimulationState() const noexcept { return m_simulationState; }

    // --- Engine Reference ---
    void setEngine(Engine* engine) noexcept { m_engine = engine; }
    [[nodiscard]] Engine* getEngine() const noexcept { return m_engine; }

private:
    Scene* m_activeScene = nullptr;
    Engine* m_engine = nullptr;

    std::vector<Entity> m_selectedEntities;
    Entity m_hoveredEntity;

    SimulationState m_simulationState = SimulationState::Stopped;
};

} // namespace bb3d::editor

#endif // BB3D_ENABLE_EDITOR
