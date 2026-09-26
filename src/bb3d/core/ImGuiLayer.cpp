#include "bb3d/core/ImGuiLayer.hpp"

#if defined(BB3D_ENABLE_EDITOR)

#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/core/Window.hpp"
#include "bb3d/core/Log.hpp"
#include "bb3d/core/IconsFontAwesome6.h"
#include "bb3d/scene/Scene.hpp"
#include "bb3d/core/portable-file-dialogs.h"
#include "bb3d/core/Engine.hpp"
#include "bb3d/render/RenderTarget.hpp"
#include "bb3d/render/SwapChain.hpp"
#include "bb3d/editor/EditorStyle.hpp"
#include "bb3d/editor/panels/SceneHierarchyPanel.hpp"
#include "bb3d/editor/panels/InspectorPanel.hpp"
#include "bb3d/editor/panels/ViewportPanel.hpp"
#include "bb3d/editor/panels/ToolbarPanel.hpp"
#include "bb3d/editor/panels/SceneSettingsPanel.hpp"
#include "bb3d/editor/panels/ConsolePanel.hpp"

#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <imgui_freetype.h>
#include <imgui_internal.h>

#include <SDL3/SDL.h>

namespace bb3d {

ImGuiLayer::ImGuiLayer(VulkanContext& context, Window& window, SwapChain& swapChain)
    : m_context(context), m_window(window), m_panelManager(m_editorContext) {

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Apply High-Performance Dark Pro Theme (Unreal Engine 5 & Blender 4 Design System)
    editor::EditorStyle::ApplyDarkProTheme();

    initFonts();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForVulkan(window.GetNativeWindow());

    // 1. Create a dedicated DescriptorPool for ImGui
    std::array<vk::DescriptorPoolSize, 1> poolSizes = {
        vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 1000)
    };
    vk::DescriptorPoolCreateInfo poolInfo({}, 1000, static_cast<uint32_t>(poolSizes.size()), poolSizes.data());
    poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    m_descriptorPool = m_context.getDevice().createDescriptorPool(poolInfo);

    // 2. Initialize ImGui Vulkan backend using Dynamic Rendering
    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance = static_cast<VkInstance>(m_context.getInstance());
    initInfo.PhysicalDevice = static_cast<VkPhysicalDevice>(m_context.getPhysicalDevice());
    initInfo.Device = static_cast<VkDevice>(m_context.getDevice());
    initInfo.QueueFamily = m_context.getGraphicsQueueFamily();
    initInfo.Queue = static_cast<VkQueue>(m_context.getGraphicsQueue());
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = static_cast<VkDescriptorPool>(m_descriptorPool);
    initInfo.MinImageCount = static_cast<uint32_t>(swapChain.getImageCount());
    initInfo.ImageCount = static_cast<uint32_t>(swapChain.getImageCount());
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.UseDynamicRendering = true;
    initInfo.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    VkFormat colorFormat = static_cast<VkFormat>(swapChain.getImageFormat());
    initInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    initInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;

    ImGui_ImplVulkan_Init(&initInfo);

    // Register modular default panels
    m_viewportPanel = m_panelManager.addPanel<editor::ViewportPanel>();
    m_panelManager.addPanel<editor::SceneHierarchyPanel>();
    m_panelManager.addPanel<editor::InspectorPanel>();
    m_panelManager.addPanel<editor::ToolbarPanel>();
    m_panelManager.addPanel<editor::SceneSettingsPanel>();
    m_panelManager.addPanel<editor::ConsolePanel>();

    BB_CORE_INFO("ImGuiLayer: Initialized with Dark Pro Design System and 6 modular editor panels.");
}

ImGuiLayer::~ImGuiLayer() {
    m_context.getDevice().waitIdle();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    m_context.getDevice().destroyDescriptorPool(m_descriptorPool);
    ImGui::DestroyContext();
}

void ImGuiLayer::initFonts() {
    ImGuiIO& io = ImGui::GetIO();
    float baseFontSize = 14.0f;
    float iconFontSize = 13.0f;

    ImFontConfig fontConfig;
    fontConfig.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint;

    m_fontRoboto = io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto-Regular.ttf", baseFontSize, &fontConfig);
    if (!m_fontRoboto) {
        m_fontRoboto = io.Fonts->AddFontDefault();
    }

    // Merge FontAwesome icons into primary font
    static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    icons_config.GlyphMinAdvanceX = iconFontSize;
    icons_config.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint;

    m_fontAwesome = io.Fonts->AddFontFromFileTTF("assets/fonts/fa-solid-900.ttf", iconFontSize, &icons_config, icons_ranges);

    // Fallback emoji and symbol fonts
    static const ImWchar full_unicode_ranges[] = { 0x0020, 0xFFFF, 0 };
    ImFontConfig fallback_config;
    fallback_config.MergeMode = true;
    fallback_config.PixelSnapH = true;
    fallback_config.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint;

    m_fontSegoe = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguiemj.ttf", baseFontSize, &fallback_config, full_unicode_ranges);
    m_fontNoto = io.Fonts->AddFontFromFileTTF("assets/fonts/NotoColorEmoji.ttf", baseFontSize, &fallback_config, full_unicode_ranges);
}

void ImGuiLayer::beginFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::endFrame(vk::CommandBuffer commandBuffer) {
    ImGui::Render();
    if (commandBuffer) {
        ImDrawData* drawData = ImGui::GetDrawData();
        if (drawData) {
            ImGui_ImplVulkan_RenderDrawData(drawData, static_cast<VkCommandBuffer>(commandBuffer));
        }
    }
}

void ImGuiLayer::onEvent(const SDL_Event& event) {
    ImGui_ImplSDL3_ProcessEvent(reinterpret_cast<const ::SDL_Event*>(&event));
    m_panelManager.onEvent(event);
}

bool ImGuiLayer::wantCaptureMouse() const {
    return ImGui::GetIO().WantCaptureMouse;
}

bool ImGuiLayer::wantCaptureKeyboard() const {
    return ImGui::GetIO().WantCaptureKeyboard;
}

ImTextureID ImGuiLayer::addTexture(vk::Sampler sampler, vk::ImageView view, vk::ImageLayout layout) {
    return (ImTextureID)ImGui_ImplVulkan_AddTexture(sampler, view, static_cast<VkImageLayout>(layout));
}

void ImGuiLayer::beginDockspace() {
    static bool dockspaceOpen = true;
    static bool opt_fullscreen = true;
    static bool opt_padding = false;
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None | ImGuiDockNodeFlags_PassthruCentralNode;

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    if (opt_fullscreen) {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    }

    if (!opt_padding) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    }

    ImGui::Begin("DockSpace Demo", &dockspaceOpen, window_flags);

    if (!opt_padding) ImGui::PopStyleVar();
    if (opt_fullscreen) ImGui::PopStyleVar(2);

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");

        // Layout initial par défaut
        if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

            ImGuiID dock_main_id = dockspace_id;
            ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, nullptr, &dock_main_id);
            ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
            ImGuiID dock_id_left_bottom = ImGui::DockBuilderSplitNode(dock_id_left, ImGuiDir_Down, 0.35f, nullptr, &dock_id_left);
            ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.25f, nullptr, &dock_main_id);
            ImGuiID dock_id_toolbar = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Up, 0.06f, nullptr, &dock_main_id);

            ImGui::DockBuilderDockWindow(ICON_FA_SITEMAP " Hierarchy", dock_id_left);
            ImGui::DockBuilderDockWindow(ICON_FA_SLIDERS " Scene Settings", dock_id_left_bottom);
            ImGui::DockBuilderDockWindow(ICON_FA_CIRCLE_INFO " Inspector", dock_id_right);
            ImGui::DockBuilderDockWindow(ICON_FA_IMAGE " Viewport", dock_main_id);
            ImGui::DockBuilderDockWindow(ICON_FA_TERMINAL " Console", dock_id_bottom);
            ImGui::DockBuilderDockWindow("Toolbar", dock_id_toolbar);

            ImGuiDockNode* toolbarNode = ImGui::DockBuilderGetNode(dock_id_toolbar);
            if (toolbarNode) {
                toolbarNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
            }

            ImGui::DockBuilderFinish(dockspace_id);
        }

        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
    }
}

void ImGuiLayer::endDockspace() {
    ImGui::End();
}

void ImGuiLayer::showMainMenu() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            auto* engine = m_editorContext.getEngine();
            if (engine) {
                if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " Save Scene")) {
                    auto res = pfd::save_file("Save Scene", "scene.json", { "Scene Files", "*.json" }).result();
                    if (!res.empty()) engine->exportScene(res);
                }
                if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Load Scene")) {
                    auto res = pfd::open_file("Load Scene", ".", { "Scene Files", "*.json" }).result();
                    if (!res.empty()) engine->importScene(res[0]);
                }
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_FA_XMARK " Exit")) engine->Stop();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            for (const auto& panel : m_panelManager.getPanels()) {
                if (panel) {
                    bool open = panel->isOpen();
                    std::string label = std::string(panel->getTitle());
                    if (ImGui::MenuItem(label.c_str(), nullptr, &open)) {
                        panel->setOpen(open);
                    }
                }
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void ImGuiLayer::renderPanels(RenderTarget* renderTarget, Scene& scene) {
    m_editorContext.setActiveScene(&scene);
    m_editorContext.setEngine(scene.getEngineContext());

    // Update Viewport texture if renderTarget is available
    if (m_viewportPanel && renderTarget) {
        m_viewportPanel->setRenderTarget(renderTarget);

        if (renderTarget->getColorImageView() != m_lastViewportImageView) {
            if (m_viewportTextureID) {
                ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)m_viewportTextureID);
            }
            m_lastViewportImageView = renderTarget->getColorImageView();
            m_viewportTextureID = addTexture(renderTarget->getSampler(), m_lastViewportImageView, vk::ImageLayout::eShaderReadOnlyOptimal);
            m_viewportPanel->setTextureId(m_viewportTextureID);
        }
    }

    // Render all open panels polymorphically
    m_panelManager.onImGuiRender();
}

glm::uvec2 ImGuiLayer::getViewportSize() const {
    if (m_viewportPanel) return m_viewportPanel->getViewportSize();
    return { 0, 0 };
}

bool ImGuiLayer::hasViewportSizeChanged() const {
    if (m_viewportPanel) return m_viewportPanel->hasViewportSizeChanged();
    return false;
}

void ImGuiLayer::clearViewportSizeChanged() {
    if (m_viewportPanel) m_viewportPanel->clearViewportSizeChanged();
}

bool ImGuiLayer::isViewportFocused() const {
    if (m_viewportPanel) return m_viewportPanel->isFocused();
    return false;
}

bool ImGuiLayer::isViewportHovered() const {
    if (m_viewportPanel) return m_viewportPanel->isHovered();
    return false;
}

} // namespace bb3d

#endif // BB3D_ENABLE_EDITOR
