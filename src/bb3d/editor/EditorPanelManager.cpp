#include "bb3d/editor/EditorPanelManager.hpp"

#if defined(BB3D_ENABLE_EDITOR)

#include <algorithm>

namespace bb3d::editor {

EditorPanelManager::EditorPanelManager(EditorContext& context)
    : m_context(context) {
    m_panels.reserve(16);
}

EditorPanelManager::~EditorPanelManager() {
    for (auto it = m_panels.rbegin(); it != m_panels.rend(); ++it) {
        if (*it) {
            (*it)->onDetach();
        }
    }
    m_panels.clear();
}

void EditorPanelManager::removePanel(std::string_view id) {
    auto it = std::find_if(m_panels.begin(), m_panels.end(), [id](const Scope<EditorPanel>& p) {
        return p && p->getId() == id;
    });

    if (it != m_panels.end()) {
        (*it)->onDetach();
        m_panels.erase(it);
    }
}

EditorPanel* EditorPanelManager::getPanel(std::string_view id) const {
    for (const auto& panel : m_panels) {
        if (panel && panel->getId() == id) {
            return panel.get();
        }
    }
    return nullptr;
}

void EditorPanelManager::onUpdate(float deltaTime) {
    for (const auto& panel : m_panels) {
        if (panel && panel->isOpen()) {
            panel->onUpdate(deltaTime);
        }
    }
}

void EditorPanelManager::onImGuiRender() {
    for (const auto& panel : m_panels) {
        if (panel && panel->isOpen()) {
            panel->onImGuiRender();
        }
    }
}

void EditorPanelManager::onEvent(const SDL_Event& event) {
    for (const auto& panel : m_panels) {
        if (panel && panel->isOpen()) {
            panel->onEvent(event);
        }
    }
}

} // namespace bb3d::editor

#endif // BB3D_ENABLE_EDITOR
