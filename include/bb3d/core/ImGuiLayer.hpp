#pragma once

#if defined(BB3D_ENABLE_EDITOR)

#include "bb3d/core/Base.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/editor/EditorContext.hpp"
#include "bb3d/editor/EditorPanelManager.hpp"
#include "bb3d/editor/panels/ViewportPanel.hpp"
#include <vulkan/vulkan.hpp>
#include <imgui.h>

struct SDL_Window;
union SDL_Event;

namespace bb3d {

class VulkanContext;
class Window;
class SwapChain;
class RenderTarget;
class Scene;

/**
 * @brief User interface layer managing Dear ImGui backend (SDL3/Vulkan) and editor panel orchestration.
 *
 * Configured with the Dark Pro Design System and delegates window rendering to modular EditorPanels.
 */
class ImGuiLayer {
public:
    /**
     * @brief Initializes ImGui context for SDL3 and Vulkan.
     * @param context Vulkan context for GPU resources.
     * @param window SDL3 window for native event loop hooks.
     * @param swapChain Active swapchain.
     */
    ImGuiLayer(VulkanContext& context, Window& window, SwapChain& swapChain);

    /** @brief Cleans up ImGui descriptor pool and context. */
    ~ImGuiLayer();

    /** @brief Prepares a new ImGui frame. */
    void beginFrame();

    /** 
     * @brief Finishes the frame and records ImGui draw commands into the command buffer.
     * @param commandBuffer Active Vulkan command buffer. If null, frame is finished without rendering.
     */
    void endFrame(vk::CommandBuffer commandBuffer = nullptr);

    /** @brief Forwards native SDL3 events to ImGui and editor panels. */
    void onEvent(const union SDL_Event& event);

    /** @brief Indicates if ImGui wants mouse input. */
    [[nodiscard]] bool wantCaptureMouse() const;

    /** @brief Indicates if ImGui wants keyboard input. */
    [[nodiscard]] bool wantCaptureKeyboard() const;

    /** @brief Begins the editor main dockspace. Must be called before panel rendering. */
    void beginDockspace();

    /** @brief Ends the editor main dockspace. Must be called after panel rendering. */
    void endDockspace();

    /** @brief Registers a Vulkan texture for use with ImGui::Image. */
    ImTextureID addTexture(vk::Sampler sampler, vk::ImageView view, vk::ImageLayout layout);

    /** @brief Renders the main top menu bar (File -> Save, Load, Exit). */
    void showMainMenu();

    /** @brief Updates the active scene and renders all registered editor panels. */
    void renderPanels(RenderTarget* renderTarget, Scene& scene);

    // --- Access to Modular Infrastructure ---
    [[nodiscard]] editor::EditorContext& getContext() noexcept { return m_editorContext; }
    [[nodiscard]] const editor::EditorContext& getContext() const noexcept { return m_editorContext; }
    [[nodiscard]] editor::EditorPanelManager& getPanelManager() noexcept { return m_panelManager; }
    [[nodiscard]] const editor::EditorPanelManager& getPanelManager() const noexcept { return m_panelManager; }

    // --- Backward Compatibility Helpers for Engine.cpp ---
    [[nodiscard]] Entity getSelectedEntity() const { return m_editorContext.getSelectedEntity(); }
    void setSelectedEntity(Entity entity) { m_editorContext.selectEntity(entity); }
    [[nodiscard]] Entity getHoveredEntity() const { return m_editorContext.getHoveredEntity(); }

    [[nodiscard]] glm::uvec2 getViewportSize() const;
    [[nodiscard]] bool hasViewportSizeChanged() const;
    void clearViewportSizeChanged();

    [[nodiscard]] bool isViewportFocused() const;
    [[nodiscard]] bool isViewportHovered() const;

    /** @name Font Accessors @{ */
    ImFont* getFontRoboto() { return m_fontRoboto; }
    ImFont* getFontAwesome() { return m_fontAwesome; }
    ImFont* getFontSegoe() { return m_fontSegoe; }
    ImFont* getFontNoto() { return m_fontNoto; }
    /** @} */

private:
    void initFonts();

    VulkanContext& m_context;
    Window& m_window;
    vk::DescriptorPool m_descriptorPool;

    ImFont* m_fontRoboto = nullptr;
    ImFont* m_fontAwesome = nullptr;
    ImFont* m_fontSegoe = nullptr;
    ImFont* m_fontNoto = nullptr;

    editor::EditorContext m_editorContext;
    editor::EditorPanelManager m_panelManager;

    editor::ViewportPanel* m_viewportPanel = nullptr;
    vk::ImageView m_lastViewportImageView = nullptr;
    ImTextureID m_viewportTextureID = 0;
};

} // namespace bb3d

#endif // BB3D_ENABLE_EDITOR