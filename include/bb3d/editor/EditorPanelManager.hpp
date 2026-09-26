#pragma once

#if defined(BB3D_ENABLE_EDITOR)

#include "bb3d/core/Base.hpp"
#include "bb3d/editor/EditorPanel.hpp"
#include <vector>
#include <string_view>
#include <typeinfo>
#include <utility>

union SDL_Event;

namespace bb3d::editor {

class EditorContext;

/**
 * @brief Manages the lifecycle, updating, and ImGui rendering of all registered EditorPanels.
 */
class EditorPanelManager {
public:
    explicit EditorPanelManager(EditorContext& context);
    ~EditorPanelManager();

    EditorPanelManager(const EditorPanelManager&) = delete;
    EditorPanelManager& operator=(const EditorPanelManager&) = delete;

    /**
     * @brief Instantiates and registers a new panel. Calls onAttach(context).
     * @tparam T Concrete panel type deriving from EditorPanel.
     * @return Pointer to the newly created panel.
     */
    template <typename T, typename... Args>
    T* addPanel(Args&&... args) {
        auto panel = CreateScope<T>(std::forward<Args>(args)...);
        T* rawPtr = panel.get();
        panel->onAttach(m_context);
        m_panels.push_back(std::move(panel));
        return rawPtr;
    }

    /** @brief Unregisters and destroys a panel by its unique ID string. Calls onDetach(). */
    void removePanel(std::string_view id);

    /** @brief Looks up a panel by its unique ID. Returns nullptr if not found. */
    [[nodiscard]] EditorPanel* getPanel(std::string_view id) const;

    /** @brief Looks up the first panel matching the given concrete C++ type. */
    template <typename T>
    [[nodiscard]] T* getPanel() const {
        for (const auto& panel : m_panels) {
            if (auto* typed = dynamic_cast<T*>(panel.get())) {
                return typed;
            }
        }
        return nullptr;
    }

    /** @brief Dispatches onUpdate() to all registered panels. */
    void onUpdate(float deltaTime);

    /** @brief Dispatches onImGuiRender() to all open panels. */
    void onImGuiRender();

    /** @brief Dispatches SDL3 native events to all registered panels. */
    void onEvent(const SDL_Event& event);

    /** @brief Read-only access to all registered panels. */
    [[nodiscard]] const std::vector<Scope<EditorPanel>>& getPanels() const noexcept { return m_panels; }

private:
    EditorContext& m_context;
    std::vector<Scope<EditorPanel>> m_panels;
};

} // namespace bb3d::editor

#endif // BB3D_ENABLE_EDITOR
