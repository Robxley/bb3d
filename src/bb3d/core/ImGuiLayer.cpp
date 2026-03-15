#include "bb3d/core/ImGuiLayer.hpp"

#if defined(BB3D_ENABLE_EDITOR)

#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/core/Window.hpp"
#include "bb3d/core/Log.hpp"
#include "bb3d/core/IconsFontAwesome6.h"
#include "bb3d/scene/Scene.hpp"
#include "bb3d/core/portable-file-dialogs.h"
#include "bb3d/core/Engine.hpp"
#include "bb3d/render/MeshGenerator.hpp"
#include "bb3d/render/TextureGenerator.hpp"
#include "bb3d/physics/PhysicsWorld.hpp"

#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <imgui_freetype.h>
#include <imgui_internal.h>

#include <SDL3/SDL.h>
#include <filesystem>

namespace bb3d {

ImGuiLayer::ImGuiLayer(VulkanContext& context, Window& window, SwapChain& swapChain)
    : m_context(context), m_window(window) {
    
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // On désactive explicitement les viewports multiples (fenêtres natives séparées) 
    // car le renderer ne gère pas encore les swapchains multiples.
    io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
    
    ImGui::StyleColorsDark();

    // Create a dedicated DescriptorPool for ImGui to handle font and viewport textures.
    std::vector<vk::DescriptorPoolSize> poolSizes = {
        {vk::DescriptorType::eCombinedImageSampler, 500}
    };
    m_descriptorPool = m_context.getDevice().createDescriptorPool({
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 500, (uint32_t)poolSizes.size(), poolSizes.data()
    });

    ImGui_ImplSDL3_InitForVulkan(m_window.GetNativeWindow());

    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance = m_context.getInstance();
    initInfo.PhysicalDevice = m_context.getPhysicalDevice();
    initInfo.Device = m_context.getDevice();
    initInfo.QueueFamily = m_context.getGraphicsQueueFamily();
    initInfo.Queue = m_context.getGraphicsQueue();
    initInfo.DescriptorPool = m_descriptorPool;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = static_cast<uint32_t>(swapChain.getImageCount());
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.UseDynamicRendering = true;
    
    initInfo.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    // We use the same format as the engine swapchain (SRGB).
    VkFormat colorFormat = static_cast<VkFormat>(vk::Format::eB8G8R8A8Srgb); 
    initInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    initInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;

    ImGui_ImplVulkan_Init(&initInfo);

    initFonts();
}

ImGuiLayer::~ImGuiLayer() {
    m_context.getDevice().waitIdle();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    m_context.getDevice().destroyDescriptorPool(m_descriptorPool);
}

void ImGuiLayer::initFonts() {
    ImGuiIO& io = ImGui::GetIO();
    // Enable FreeType for high-quality font rendering and color glyph support.
    io.Fonts->FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;

    const float fontSize = 18.0f;
    const float emojiSize = 24.0f;

    // 1. Roboto (Base font)
    const char* robotoPath = "assets/fonts/Roboto-Regular.ttf";
    if (std::filesystem::exists(robotoPath)) {
        m_fontRoboto = io.Fonts->AddFontFromFileTTF(robotoPath, fontSize);
    } else {
        m_fontRoboto = io.Fonts->AddFontDefault();
        BB_CORE_WARN("ImGui: Roboto font not found at {}, using default font.", robotoPath);
    }

    // 2. FontAwesome (Merged into base for technical icons)
    ImFontConfig iconConfig;
    iconConfig.MergeMode = true;
    iconConfig.PixelSnapH = true;
    iconConfig.GlyphMinAdvanceX = fontSize;
    static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    const char* faPath = "assets/fonts/fa-solid-900.ttf";
    if (std::filesystem::exists(faPath)) {
        m_fontAwesome = io.Fonts->AddFontFromFileTTF(faPath, fontSize, &iconConfig, icon_ranges);
    }

    // 3. Segoe UI Emoji (Merged into base for color emojis)
    ImFontConfig emojiConfig;
    emojiConfig.MergeMode = true;
    emojiConfig.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;
    static const ImWchar emoji_ranges[] = { 0x1, 0x1FFFF, 0 };
    const char* segoePath = "C:/Windows/Fonts/seguiemj.ttf";
    if (std::filesystem::exists(segoePath)) {
        m_fontSegoe = io.Fonts->AddFontFromFileTTF(segoePath, emojiSize, &emojiConfig, emoji_ranges);
    }

    ImGui_ImplVulkan_CreateFontsTexture();
}

void ImGuiLayer::beginFrame() {
    BB_CORE_TRACE("ImGuiLayer::beginFrame()");
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::endFrame(vk::CommandBuffer commandBuffer) {
    if (commandBuffer) {
        BB_CORE_TRACE("ImGuiLayer::endFrame(with CB)");
        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), static_cast<VkCommandBuffer>(commandBuffer));
    } else {
        BB_CORE_TRACE("ImGuiLayer::endFrame(no CB)");
        ImGui::EndFrame();
    }
}

void ImGuiLayer::beginDockspace() {
    static bool dockspaceOpen = true;
    static bool opt_fullscreen = true;
    static bool opt_padding = false;
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

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

    if (!opt_padding)
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("DockSpace Demo", &dockspaceOpen, window_flags);

    if (!opt_padding)
        ImGui::PopStyleVar();

    if (opt_fullscreen)
        ImGui::PopStyleVar(2);

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        
        // Auto-layout on first launch
        if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

            ImGuiID dock_main_id = dockspace_id;
            ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, nullptr, &dock_main_id);
            ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
            ImGuiID dock_id_left_bottom = ImGui::DockBuilderSplitNode(dock_id_left, ImGuiDir_Down, 0.30f, nullptr, &dock_id_left);
            ImGuiID dock_id_toolbar = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Up, 0.06f, nullptr, &dock_main_id);

            ImGui::DockBuilderDockWindow(ICON_FA_SITEMAP " Hierarchy", dock_id_left);
            ImGui::DockBuilderDockWindow(ICON_FA_SLIDERS " Scene Settings", dock_id_left_bottom);
            ImGui::DockBuilderDockWindow(ICON_FA_CIRCLE_INFO " Inspector", dock_id_right);
            ImGui::DockBuilderDockWindow(ICON_FA_IMAGE " Viewport", dock_main_id);
            ImGui::DockBuilderDockWindow("Toolbar", dock_id_toolbar);
            
            // Hide the tab bar for the toolbar to make it look seamless
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

void ImGuiLayer::onEvent(const SDL_Event& event) {
    ImGui_ImplSDL3_ProcessEvent(reinterpret_cast<const ::SDL_Event*>(&event));
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

void ImGuiLayer::showViewport(RenderTarget* renderTarget, Scene& /*scene*/) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
    ImGui::Begin(ICON_FA_IMAGE " Viewport");

    if (renderTarget) {
        // Enregistrement de la texture si elle a changé (ex: resize)
        if (renderTarget->getColorImageView() != m_lastViewportImageView) {
            if (m_viewportTextureID) {
                ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)m_viewportTextureID);
            }
            m_lastViewportImageView = renderTarget->getColorImageView();
            m_viewportTextureID = addTexture(renderTarget->getSampler(), m_lastViewportImageView, vk::ImageLayout::eShaderReadOnlyOptimal);
        }

        ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
        
        // On enregistre la taille souhaitée pour le prochain rendu
        uint32_t width = static_cast<uint32_t>(viewportPanelSize.x);
        uint32_t height = static_cast<uint32_t>(viewportPanelSize.y);
        
        if (width > 0 && height > 0 && (width != m_viewportSize.x || height != m_viewportSize.y)) {
            // Sécurité : on ne demande un resize QUE si c'est différent de la taille réelle actuelle du RT
            if (width != renderTarget->getExtent().width || height != renderTarget->getExtent().height) {
                m_viewportSize = { width, height };
                m_viewportSizeChanged = true;
            } else {
                // Si la taille match déjà, on reset notre état interne sans trigger de resize
                m_viewportSize = { width, height };
                m_viewportSizeChanged = false;
            }
        }

        m_viewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        m_viewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

        if (m_viewportTextureID) {
            ImGui::Image(m_viewportTextureID, viewportPanelSize);
        }
    } else {
        ImGui::Text("No RenderTarget available (Offscreen Rendering disabled?)");
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void ImGuiLayer::showSceneHierarchy(Scene& scene) {
    ImGui::Begin(ICON_FA_SITEMAP " Hierarchy");

    m_hoveredEntity = {}; // Reset at start of frame

    // Categories
    std::vector<Entity> cameras;
    std::vector<Entity> environment;
    std::vector<Entity> renderables;
    std::vector<Entity> audio;
    std::vector<Entity> logicAndPhysics;
    std::vector<Entity> other;

    // Categorization Pass
    scene.getRegistry().view<entt::entity>().each([&](auto entityHandle) {
        Entity entity(entityHandle, scene);
        
        if (entity.has<CameraComponent>()) {
            cameras.push_back(entity);
        } else if (entity.has<LightComponent>() || entity.has<SkyboxComponent>() || entity.has<SkySphereComponent>() || entity.has<TerrainComponent>() || entity.has<ParticleSystemComponent>()) {
            environment.push_back(entity);
        } else if (entity.has<MeshComponent>() || entity.has<ModelComponent>()) {
            renderables.push_back(entity);
        } else if (entity.has<AudioSourceComponent>() || entity.has<AudioListenerComponent>()) {
            audio.push_back(entity);
        } else if (entity.has<PhysicsComponent>() || entity.has<NativeScriptComponent>()) {
            logicAndPhysics.push_back(entity);
        } else {
            other.push_back(entity);
        }
    });

    auto drawCategory = [&](const char* title, const char* icon, const std::vector<Entity>& entities) {
        if (entities.empty()) return;
        
        // Push a distinct ID for the header
        ImGui::PushID(title);
        ImGuiTreeNodeFlags headerFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
        bool open = ImGui::CollapsingHeader(std::string(std::string(icon) + " " + title).c_str(), headerFlags);
        if (open) {
            for (auto e : entities) {
                drawEntityNode(e);
            }
        }
        ImGui::PopID();
    };

    // Render Categories
    drawCategory("Cameras", ICON_FA_VIDEO, cameras);
    drawCategory("Environment", ICON_FA_SUN, environment);
    drawCategory("3D Objects", ICON_FA_CUBES, renderables);
    drawCategory("Audio", ICON_FA_MUSIC, audio);
    drawCategory("Physics & Logic", ICON_FA_BOLT, logicAndPhysics);
    drawCategory("Other", ICON_FA_FILE, other);

    // Deselect if clicking on empty space.
    if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered()) {
        m_selectedEntity = {};
    }

    // Context menu for entity creation.
    if (ImGui::BeginPopupContextWindow(0, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        static int entityCount = 0;
        
        if (ImGui::MenuItem(ICON_FA_PLUS " Create Empty Entity")) {
            scene.createEntity("Entity " + std::to_string(entityCount++));
        }

        if (ImGui::MenuItem(ICON_FA_FILE_IMPORT " Import 3D Model...")) {
            auto selection = pfd::open_file("Import 3D Model", ".", { "3D Models", "*.gltf *.glb *.obj" }).result();
            if (!selection.empty()) {
                std::filesystem::path path(selection[0]);
                std::string name = path.stem().string();
                // Create entity and normalize its size to 1.0 units for better visibility initially
                auto view = scene.createModelEntity(name, selection[0], {0,0,0}, {1.0f, 1.0f, 1.0f});
                BB_CORE_INFO("Editor: Imported model {0} from {1}", name, selection[0]);
            }
        }

        if (ImGui::BeginMenu(ICON_FA_SHAPES " Create Primitive")) {
            auto assignDefaultMat = [&](Entity e) {
                if (e.has<MeshComponent>()) {
                    auto mesh = e.get<MeshComponent>().mesh;
                    if (mesh && !mesh->getMaterial()) {
                        mesh->setMaterial(CreateRef<PBRMaterial>(m_context));
                    }
                }
            };

            if (ImGui::MenuItem("Cube")) {
                auto e = scene.createEntity("Cube " + std::to_string(entityCount++));
                e.add<MeshComponent>(MeshGenerator::createCube(m_context, 1.0f), "", PrimitiveType::Cube);
                assignDefaultMat(e);
            }
            if (ImGui::MenuItem("Sphere")) {
                auto e = scene.createEntity("Sphere " + std::to_string(entityCount++));
                e.add<MeshComponent>(MeshGenerator::createSphere(m_context, 0.5f, 32), "", PrimitiveType::Sphere);
                assignDefaultMat(e);
            }
            if (ImGui::MenuItem("Plane")) {
                auto e = scene.createEntity("Plane " + std::to_string(entityCount++));
                e.add<MeshComponent>(MeshGenerator::createCheckerboardPlane(m_context, 10.0f, 10), "", PrimitiveType::Plane);
                auto mesh = e.get<MeshComponent>().mesh;
                mesh->setMaterial(CreateRef<PBRMaterial>(m_context));
            }
            ImGui::EndMenu();
        }

        ImGui::EndPopup();
    }

    ImGui::End();
}

void ImGuiLayer::drawEntityNode(Entity entity) {
    auto& tag = entity.get<TagComponent>().tag;
    ImGui::PushID((int)(entt::entity)entity);

    ImGuiTreeNodeFlags flags = ((m_selectedEntity == entity && m_focusedComponent.empty()) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
    flags |= ImGuiTreeNodeFlags_SpanAvailWidth;

    const char* icon = ICON_FA_FILE;
    ImVec4 iconColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    if (entity.has<CameraComponent>()) {
        if (entity.has<FPSControllerComponent>()) icon = ICON_FA_VIDEO;
        else if (entity.has<OrbitControllerComponent>()) icon = ICON_FA_GLOBE;
        else icon = ICON_FA_CAMERA;
        iconColor = ImVec4(0.3f, 0.8f, 1.0f, 1.0f);
    } else if (entity.has<LightComponent>()) {
        auto type = entity.get<LightComponent>().type;
        if (type == LightType::Directional) icon = ICON_FA_SUN;
        else icon = ICON_FA_LIGHTBULB;
        iconColor = ImVec4(1.0f, 0.9f, 0.2f, 1.0f);
    } else if (entity.has<TerrainComponent>()) {
        icon = ICON_FA_MOUNTAIN;
        iconColor = ImVec4(0.2f, 0.8f, 0.3f, 1.0f);
    } else if (entity.has<ParticleSystemComponent>()) {
        icon = ICON_FA_WAND_MAGIC_SPARKLES;
        iconColor = ImVec4(1.0f, 0.4f, 0.8f, 1.0f);
    } else if (entity.has<SkyboxComponent>() || entity.has<SkySphereComponent>()) {
        icon = ICON_FA_CLOUD;
        iconColor = ImVec4(0.6f, 0.8f, 0.9f, 1.0f);
    } else if (entity.has<AudioListenerComponent>() || entity.has<AudioSourceComponent>()) {
        icon = entity.has<AudioListenerComponent>() ? ICON_FA_HEADPHONES : ICON_FA_VOLUME_HIGH;
        iconColor = ImVec4(1.0f, 0.6f, 0.2f, 1.0f);
    } else if (entity.has<ModelComponent>() || entity.has<MeshComponent>()) {
        icon = entity.has<ModelComponent>() ? ICON_FA_CUBES : ICON_FA_CUBE;
        iconColor = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
    } else if (entity.has<PhysicsComponent>()) {
        icon = ICON_FA_BOX;
        iconColor = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
    } else if (entity.has<NativeScriptComponent>()) {
        icon = ICON_FA_CODE;
        iconColor = ImVec4(0.6f, 0.4f, 1.0f, 1.0f);
    }

    bool node_open = ImGui::TreeNodeEx("##node", flags, "");
    if (ImGui::IsItemHovered()) m_hoveredEntity = entity;
    if (ImGui::IsItemClicked()) { m_selectedEntity = entity; m_focusedComponent = ""; }

    ImGui::SameLine();
    ImGui::TextColored(iconColor, "%s", icon);
    ImGui::SameLine();
    ImGui::Text("%s", tag.c_str());

    if (node_open) {
        auto drawCompNode = [&](const char* name, const char* compIcon, ImVec4 color, bool hasComp) {
            if (!hasComp) return;
            ImGuiTreeNodeFlags compFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (m_selectedEntity == entity && m_focusedComponent == name) compFlags |= ImGuiTreeNodeFlags_Selected;
            ImGui::TreeNodeEx((std::string("##Comp") + name).c_str(), compFlags, "");
            if (ImGui::IsItemClicked()) { m_selectedEntity = entity; m_focusedComponent = name; }
            ImGui::SameLine();
            ImGui::TextColored(color, "%s", compIcon);
            ImGui::SameLine();
            ImGui::Text("%s", name);
        };

        ImVec4 colTransform = ImVec4(0.3f, 0.8f, 1.0f, 1.0f);
        ImVec4 colRender = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
        ImVec4 colPhysics = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
        ImVec4 colLight = ImVec4(1.0f, 0.9f, 0.2f, 1.0f);
        ImVec4 colAudio = ImVec4(1.0f, 0.6f, 0.2f, 1.0f);
        ImVec4 colLogic = ImVec4(0.6f, 0.4f, 1.0f, 1.0f);

        drawCompNode("Transform", ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT, colTransform, entity.has<TransformComponent>());
        drawCompNode("Camera", ICON_FA_VIDEO, colTransform, entity.has<CameraComponent>());
        drawCompNode("OrbitController", ICON_FA_CROSSHAIRS, colTransform, entity.has<OrbitControllerComponent>());
        drawCompNode("FPSController", ICON_FA_GAMEPAD, colTransform, entity.has<FPSControllerComponent>());
        drawCompNode("Mesh", ICON_FA_CUBE, colRender, entity.has<MeshComponent>());
        drawCompNode("Model", ICON_FA_CUBES, colRender, entity.has<ModelComponent>());
        drawCompNode("Light", ICON_FA_LIGHTBULB, colLight, entity.has<LightComponent>());
        drawCompNode("Skybox", ICON_FA_CLOUD, colLight, entity.has<SkyboxComponent>());
        drawCompNode("SkySphere", ICON_FA_GLOBE, colLight, entity.has<SkySphereComponent>());
        drawCompNode("PhysicsBody", ICON_FA_BOX, colPhysics, entity.has<PhysicsComponent>());
        drawCompNode("AudioSource", ICON_FA_VOLUME_HIGH, colAudio, entity.has<AudioSourceComponent>());
        drawCompNode("AudioListener", ICON_FA_EAR_LISTEN, colAudio, entity.has<AudioListenerComponent>());
        drawCompNode("SimpleAnimation", ICON_FA_FILM, colLogic, entity.has<SimpleAnimationComponent>());
        drawCompNode("NativeScript", ICON_FA_CODE, colLogic, entity.has<NativeScriptComponent>());

        ImGui::TreePop();
    }

    if (ImGui::BeginPopupContextItem("EntityContextMenu")) {
        if (ImGui::MenuItem(ICON_FA_TRASH " Delete Entity")) {
            entity.getScene().destroyEntity(entity);
            if (m_selectedEntity == entity) m_selectedEntity = {};
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();
}

void ImGuiLayer::showSceneSettings(Scene& scene) {
    ImGui::Begin(ICON_FA_SLIDERS " Scene Settings");

    if (ImGui::CollapsingHeader(ICON_FA_CLOUD " Environment", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& fog = scene.getFog();
        FogSettings newFog = fog;
        
        const char* fogTypes[] = { "None", "Linear", "Exponential", "Exponential Squared" };
        int currentFog = static_cast<int>(fog.type);
        if (ImGui::Combo("Fog Type", &currentFog, fogTypes, IM_ARRAYSIZE(fogTypes))) {
            newFog.type = static_cast<FogType>(currentFog);
        }
        
        if (newFog.type != FogType::None) {
            ImGui::ColorEdit3("Fog Color", &newFog.color.x);
            ImGui::DragFloat("Fog Density", &newFog.density, 0.001f, 0.0f, 1.0f);
        }
        
        if (memcmp(&fog, &newFog, sizeof(FogSettings)) != 0) {
            scene.setFog(newFog);
        }
    }

    ImGui::Separator();
    static bool showGrid = true;
    ImGui::Checkbox("Show Grid", &showGrid);
    static bool showPhysics = false;
    ImGui::Checkbox("Debug Physics Colliders", &showPhysics);

    ImGui::End();
}

void ImGuiLayer::showInspector() {
    ImGui::Begin(ICON_FA_CIRCLE_INFO " Inspector");

    if (m_selectedEntity) {
        auto& tag = m_selectedEntity.get<TagComponent>().tag;

        ImVec4 colTransform = ImVec4(0.3f, 0.8f, 1.0f, 1.0f);
        ImVec4 colRender    = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
        ImVec4 colPhysics   = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
        ImVec4 colLight     = ImVec4(1.0f, 0.9f, 0.2f, 1.0f);
        ImVec4 colAudio     = ImVec4(1.0f, 0.6f, 0.2f, 1.0f);
        ImVec4 colLogic     = ImVec4(0.6f, 0.4f, 1.0f, 1.0f);
        // --- Section 1: Mini-Hierarchy (Structure) ---
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNodeEx("Structure", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth)) {
            ImGuiTreeNodeFlags entityFlags = (m_focusedComponent.empty() ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
            ImGui::TreeNodeEx((tag + "##InspectorTree").c_str(), entityFlags | ImGuiTreeNodeFlags_NoTreePushOnOpen, ICON_FA_CUBE " %s", tag.c_str());
            if (ImGui::IsItemClicked()) m_focusedComponent = "";

            auto DrawCompTreeLeaf = [&](const char* name, const char* icon, ImVec4 color, bool hasComp) {
                if (!hasComp) return;
                ImGuiTreeNodeFlags flags = (m_focusedComponent == name ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::TreeNodeEx((std::string("##Tree") + name).c_str(), flags, "%s %s", icon, name);
                ImGui::PopStyleColor();
                if (ImGui::IsItemClicked()) m_focusedComponent = name;
            };

            DrawCompTreeLeaf("Transform", ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT, colTransform, m_selectedEntity.has<TransformComponent>());
            DrawCompTreeLeaf("Camera", ICON_FA_VIDEO, colTransform, m_selectedEntity.has<CameraComponent>());
            DrawCompTreeLeaf("OrbitController", ICON_FA_CROSSHAIRS, colTransform, m_selectedEntity.has<OrbitControllerComponent>());
            DrawCompTreeLeaf("FPSController", ICON_FA_GAMEPAD, colTransform, m_selectedEntity.has<FPSControllerComponent>());
            DrawCompTreeLeaf("Mesh", ICON_FA_CUBE, colRender, m_selectedEntity.has<MeshComponent>());
            DrawCompTreeLeaf("Model", ICON_FA_CUBES, colRender, m_selectedEntity.has<ModelComponent>());
            DrawCompTreeLeaf("Light", ICON_FA_LIGHTBULB, colLight, m_selectedEntity.has<LightComponent>());
            DrawCompTreeLeaf("Skybox", ICON_FA_CLOUD, colLight, m_selectedEntity.has<SkyboxComponent>());
            DrawCompTreeLeaf("SkySphere", ICON_FA_GLOBE, colLight, m_selectedEntity.has<SkySphereComponent>());
            DrawCompTreeLeaf("PhysicsBody", ICON_FA_BOX, colPhysics, m_selectedEntity.has<PhysicsComponent>());
            DrawCompTreeLeaf("AudioSource", ICON_FA_VOLUME_HIGH, colAudio, m_selectedEntity.has<AudioSourceComponent>());
            DrawCompTreeLeaf("AudioListener", ICON_FA_EAR_LISTEN, colAudio, m_selectedEntity.has<AudioListenerComponent>());
            DrawCompTreeLeaf("SimpleAnimation", ICON_FA_FILM, colLogic, m_selectedEntity.has<SimpleAnimationComponent>());
            DrawCompTreeLeaf("NativeScript", ICON_FA_CODE, colLogic, m_selectedEntity.has<NativeScriptComponent>());
            DrawCompTreeLeaf("OrbitalGravity", ICON_FA_MAGNET, colLogic, m_selectedEntity.has<OrbitalGravityComponent>());
            DrawCompTreeLeaf("SpaceshipController", ICON_FA_ROCKET, colLogic, m_selectedEntity.has<SpaceshipControllerComponent>());

            ImGui::TreePop();
        }

        ImGui::Separator();

        // --- Section 2: Property Editor (Isolated) ---
        if (m_focusedComponent.empty()) {
            // Summary view when entity itself is selected
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Entity Summary");
            char buffer[256];
            memset(buffer, 0, sizeof(buffer));
            strcpy_s(buffer, tag.c_str());
            if (ImGui::InputText(ICON_FA_TAG " Tag", buffer, sizeof(buffer))) { tag = std::string(buffer); }
        } else {
            auto DrawMaterialUI = [&](Material* material) {
                if (!material) return;
                if (ImGui::TreeNodeEx(ICON_FA_PALETTE " Material Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto pbrMat = dynamic_cast<PBRMaterial*>(material);
                    if (pbrMat) {
                        glm::vec3 color = pbrMat->getColor();
                        if (ImGui::ColorEdit3("Albedo Color", &color.x)) {
                            pbrMat->setColor(color);
                            if (m_selectedEntity.has<MeshComponent>()) m_selectedEntity.get<MeshComponent>().color = color;
                        }
                        auto TextureSlot = [&](const char* label, bool isColor, std::function<void(Ref<Texture>)> setter) {
                            ImGui::PushID(label);
                            if (ImGui::Button(ICON_FA_FOLDER_OPEN)) {
                                auto selection = pfd::open_file("Load Texture", ".", { "Image Files", "*.png *.jpg *.jpeg *.tga *.bmp" }).result();
                                if (!selection.empty()) {
                                    try {
                                        auto tex = Engine::Get().assets().load<Texture>(selection[0], isColor);
                                        setter(tex);
                                    } catch (const std::exception& e) { BB_CORE_ERROR("Failed to load texture: {}", e.what()); }
                                }
                            }
                            ImGui::SameLine(); ImGui::Text("%s", label);
                            ImGui::PopID();
                        };
                        TextureSlot("Albedo Map", true, [&](Ref<Texture> t){ pbrMat->setAlbedoMap(t); });
                        TextureSlot("Normal Map", false, [&](Ref<Texture> t){ pbrMat->setNormalMap(t); });
                        TextureSlot("ORM Map", false, [&](Ref<Texture> t){ pbrMat->setORMMap(t); });
                        TextureSlot("Emissive Map", true, [&](Ref<Texture> t){ pbrMat->setEmissiveMap(t); });
                    }
                    ImGui::TreePop();
                }
            };

            auto ComponentPropertiesHeader = [&](const char* name, const char* icon, ImVec4 color, bool canRemove = true, std::function<void()> onRemove = nullptr) -> bool {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(color.x, color.y, color.z, 0.1f));
                ImGui::BeginChild((std::string("##Child") + name).c_str(), ImVec2(0, 32), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::Text("%s %s", icon, name);
                ImGui::PopStyleColor();

                if (canRemove && onRemove) {
                    ImGui::SameLine(ImGui::GetWindowWidth() - 35);
                    if (ImGui::Button((std::string(ICON_FA_TRASH "##") + name).c_str())) {
                        onRemove();
                        ImGui::EndChild();
                        ImGui::PopStyleColor();
                        return false; 
                    }
                }
                
                ImGui::EndChild();
                ImGui::PopStyleColor();
                
                ImGui::Spacing();
                
                return true;
            };

            if (m_focusedComponent == "Transform" && m_selectedEntity.has<TransformComponent>()) {
                if (ComponentPropertiesHeader("Transform", ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT, colTransform, false)) {
                    auto& tc = m_selectedEntity.get<TransformComponent>();
                    bool tcChanged = false;
                    if (ImGui::DragFloat3("##Pos", &tc.translation.x, 0.1f)) tcChanged = true;
                    ImGui::SameLine(); ImGui::TextColored(colTransform, "Position");
                    glm::vec3 rotDeg = glm::degrees(tc.rotation);
                    if (ImGui::DragFloat3("##Rot", &rotDeg.x, 0.1f)) { tc.rotation = glm::radians(rotDeg); tcChanged = true; }
                    ImGui::SameLine(); ImGui::TextColored(colTransform, "Rotation");
                    if (ImGui::DragFloat3("##Scale", &tc.scale.x, 0.1f)) tcChanged = true;
                    ImGui::SameLine(); ImGui::TextColored(colTransform, "Scale");

                    if (tcChanged && m_selectedEntity.has<PhysicsComponent>()) {
                        auto ph = Engine::Get().GetPhysicsWorld();
                        if (ph) ph->updateBodyTransform(m_selectedEntity);
                    }
                    if (ImGui::Button(ICON_FA_ARROW_ROTATE_LEFT " Reset")) { 
                        tc.resetToInitial(); 
                        auto ph = Engine::Get().GetPhysicsWorld();
                        if (ph) ph->resetBody(m_selectedEntity); 
                    }
                    ImGui::SameLine(); if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save")) { tc.saveInitialState(); }
                }
            } else if (m_focusedComponent == "Camera" && m_selectedEntity.has<CameraComponent>()) {
                if (ComponentPropertiesHeader("Camera", ICON_FA_VIDEO, colTransform, true, [&](){ m_selectedEntity.remove<CameraComponent>(); m_focusedComponent = ""; })) {
                    auto& cam = m_selectedEntity.get<CameraComponent>();
                    bool changed = false;
                    ImGui::Checkbox("Primary Active", &cam.active);
                    changed |= ImGui::DragFloat("FOV", &cam.fov, 0.5f, 10.0f, 120.0f);
                    changed |= ImGui::DragFloat("Near", &cam.nearPlane, 0.01f, 0.001f, 10.0f);
                    changed |= ImGui::DragFloat("Far", &cam.farPlane, 10.0f, 100.0f, 10000.0f);
                    if (changed && cam.camera) {
                        cam.camera->setPerspective(cam.fov, cam.aspect, cam.nearPlane, cam.farPlane);
                    }
                }
            } else if (m_focusedComponent == "OrbitController" && m_selectedEntity.has<OrbitControllerComponent>()) {
                if (ComponentPropertiesHeader("OrbitController", ICON_FA_CROSSHAIRS, colTransform, true, [&](){ m_selectedEntity.remove<OrbitControllerComponent>(); m_focusedComponent = ""; })) {
                    auto& oc = m_selectedEntity.get<OrbitControllerComponent>();
                    ImGui::DragFloat3("Target", &oc.target.x, 0.1f);
                    ImGui::DragFloat("Distance", &oc.distance, 0.1f, oc.minDistance, oc.maxDistance);
                    ImGui::DragFloat("Min Distance", &oc.minDistance, 0.1f, 0.1f, oc.maxDistance);
                    ImGui::DragFloat("Max Distance", &oc.maxDistance, 0.1f, oc.minDistance, 1000.0f);
                }
            } else if (m_focusedComponent == "FPSController" && m_selectedEntity.has<FPSControllerComponent>()) {
                if (ComponentPropertiesHeader("FPSController", ICON_FA_GAMEPAD, colTransform, true, [&](){ m_selectedEntity.remove<FPSControllerComponent>(); m_focusedComponent = ""; })) {
                    auto& fps = m_selectedEntity.get<FPSControllerComponent>();
                    ImGui::DragFloat3("Movement Speed", &fps.movementSpeed.x, 0.1f);
                    ImGui::DragFloat2("Rotation Speed", &fps.rotationSpeed.x, 0.01f);
                    ImGui::DragFloat("Yaw", &fps.yaw, 1.0f);
                    ImGui::DragFloat("Pitch", &fps.pitch, 1.0f, -89.0f, 89.0f);
                }
            } else if (m_focusedComponent == "Mesh" && m_selectedEntity.has<MeshComponent>()) {
                if (ComponentPropertiesHeader("Mesh", ICON_FA_CUBE, colRender)) {
                    auto& mc = m_selectedEntity.get<MeshComponent>();
                    ImGui::Checkbox("Visible", &mc.visible);
                    ImGui::Text("Primitive: %s", mc.assetPath.c_str());
                    if (mc.mesh) DrawMaterialUI(mc.mesh->getMaterial().get());
                }
            } else if (m_focusedComponent == "Model" && m_selectedEntity.has<ModelComponent>()) {
                if (ComponentPropertiesHeader("Model", ICON_FA_CUBES, colRender)) {
                    auto& mc = m_selectedEntity.get<ModelComponent>();
                    ImGui::Checkbox("Visible", &mc.visible);
                    
                    static ModelLoadConfig s_loadConfig;
                    const char* presets[] = { "Standard PBR", "Cell Shading" };
                    int currentPreset = static_cast<int>(s_loadConfig.preset);
                    if (ImGui::Combo("Loading Preset", &currentPreset, presets, IM_ARRAYSIZE(presets))) {
                        s_loadConfig.preset = static_cast<ModelLoadPreset>(currentPreset);
                    }
                    
                    if (ImGui::TreeNode("Advanced Loading Options")) {
                        ImGui::Checkbox("Load Animations", &s_loadConfig.loadAnimations);
                        ImGui::Checkbox("Load Materials", &s_loadConfig.loadMaterials);
                        ImGui::Checkbox("Load PBR Maps", &s_loadConfig.loadPBRMaps);
                        ImGui::Checkbox("Load Alpha Modes", &s_loadConfig.loadAlphaModes);
                        ImGui::Checkbox("Load Vertex Colors", &s_loadConfig.loadVertexColors);
                        ImGui::DragFloat3("Initial Scale", &s_loadConfig.initialScale.x, 0.1f);
                        ImGui::TreePop();
                    }

                    ImGui::Text("File: %s", mc.assetPath.c_str());
                    ImGui::SameLine();
                    if (ImGui::Button(ICON_FA_FOLDER_OPEN "##Load")) {
                        auto selection = pfd::open_file("Select Model", ".", { "3D Models", "*.gltf *.glb *.obj" }).result();
                        if (!selection.empty()) {
                            auto& engineContext = Engine::Get();
                            mc.model = engineContext.assets().load<Model>(selection[0], s_loadConfig);
                            mc.assetPath = selection[0];
                            mc.model->normalize(glm::vec3(1.0f));
                            BB_CORE_INFO("Editor: Swapped model to {0} with custom config", selection[0]);
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(ICON_FA_ROTATE "##Reload") && !mc.assetPath.empty()) {
                         auto& engineContext = Engine::Get();
                         mc.model = engineContext.assets().load<Model>(mc.assetPath, s_loadConfig);
                         mc.model->normalize(glm::vec3(1.0f));
                         BB_CORE_INFO("Editor: Reloaded model {0} with custom config", mc.assetPath);
                    }

                    if (mc.model && ImGui::TreeNodeEx("Materials", ImGuiTreeNodeFlags_DefaultOpen)) {
                        int matIdx = 0;
                        for (auto& mesh : mc.model->getMeshes()) {
                            ImGui::PushID(matIdx++);
                            DrawMaterialUI(mesh->getMaterial().get());
                            ImGui::PopID();
                        }
                        ImGui::TreePop();
                    }
                }
            } else if (m_focusedComponent == "Light" && m_selectedEntity.has<LightComponent>()) {
            } else if (m_focusedComponent == "Light" && m_selectedEntity.has<LightComponent>()) {
                if (ComponentPropertiesHeader("Light", ICON_FA_LIGHTBULB, colLight, true, [&](){ m_selectedEntity.remove<LightComponent>(); m_focusedComponent = ""; })) {
                    auto& light = m_selectedEntity.get<LightComponent>();
                    const char* types[] = { "Directional", "Point", "Spot" };
                    int currentType = static_cast<int>(light.type);
                    if (ImGui::Combo("Type", &currentType, types, IM_ARRAYSIZE(types))) light.type = static_cast<LightType>(currentType);
                    ImGui::ColorEdit3("Color", &light.color.x);
                    ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 100.0f);
                    if (light.type != LightType::Directional) ImGui::DragFloat("Range", &light.range, 0.1f, 0.0f, 500.0f);
                    ImGui::Checkbox("Cast Shadows", &light.castShadows);
                }
            } else if (m_focusedComponent == "Skybox" && m_selectedEntity.has<SkyboxComponent>()) {
                if (ComponentPropertiesHeader("Skybox", ICON_FA_CLOUD, colLight, true, [&](){ m_selectedEntity.remove<SkyboxComponent>(); m_focusedComponent = ""; })) {
                    auto& sky = m_selectedEntity.get<SkyboxComponent>();
                    ImGui::Text("File: %s", sky.assetPath.empty() ? "None" : sky.assetPath.c_str());
                    if (ImGui::Button(ICON_FA_FOLDER_OPEN " Load Cubemap/HDRI")) {
                        auto selection = pfd::open_file("Load Image", ".", { "Images", "*.hdr *.png *.jpg" }).result();
                        if (!selection.empty()) {
                            try {
                                sky.cubemap = Engine::Get().assets().load<Texture>(selection[0], true);
                                sky.assetPath = selection[0];
                            } catch(...) {}
                        }
                    }
                }
            } else if (m_focusedComponent == "SkySphere" && m_selectedEntity.has<SkySphereComponent>()) {
                if (ComponentPropertiesHeader("SkySphere", ICON_FA_GLOBE, colLight, true, [&](){ m_selectedEntity.remove<SkySphereComponent>(); m_focusedComponent = ""; })) {
                    auto& sky = m_selectedEntity.get<SkySphereComponent>();
                    ImGui::Text("File: %s", sky.assetPath.empty() ? "None" : sky.assetPath.c_str());
                    if (ImGui::Button(ICON_FA_FOLDER_OPEN " Load Sphere Map")) {
                        auto selection = pfd::open_file("Load Image", ".", { "Images", "*.hdr *.png *.jpg" }).result();
                        if (!selection.empty()) {
                            try {
                                sky.texture = Engine::Get().assets().load<Texture>(selection[0], true);
                                sky.assetPath = selection[0];
                            } catch(...) {}
                        }
                    }
                    ImGui::Checkbox("Flip Y-Axis", &sky.flipY);
                }
            } else if (m_focusedComponent == "PhysicsBody" && m_selectedEntity.has<PhysicsComponent>()) {
                if (ComponentPropertiesHeader("PhysicsBody", ICON_FA_BOX, colPhysics, true, [&](){ 
                    m_selectedEntity.getScene().getEngineContext()->physics().destroyRigidBody(m_selectedEntity);
                    m_selectedEntity.remove<PhysicsComponent>(); 
                    m_focusedComponent = ""; 
                })) {
                    auto& phys = m_selectedEntity.get<PhysicsComponent>();
                    bool changed = false;
                    
                    const char* bodyTypes[] = { "Static", "Dynamic", "Kinematic" };
                    int typeIdx = static_cast<int>(phys.type);
                    if (ImGui::Combo("Body Type", &typeIdx, bodyTypes, IM_ARRAYSIZE(bodyTypes))) {
                        phys.type = static_cast<BodyType>(typeIdx);
                        changed = true;
                    }

                    changed |= ImGui::DragFloat("Mass", &phys.mass, 0.1f, 0.0f, 1000.0f);
                    changed |= ImGui::SliderFloat("Friction", &phys.friction, 0.0f, 1.0f);
                    changed |= ImGui::SliderFloat("Restitution", &phys.restitution, 0.0f, 1.0f);
                    changed |= ImGui::Checkbox("Constrain X-Y Plane", &phys.constrain2D);
                    
                    ImGui::Separator();
                    const char* colTypes[] = { "Box", "Sphere", "Capsule", "Mesh" };
                    int colIdx = static_cast<int>(phys.colliderType);
                    if (ImGui::Combo("Collider Shape", &colIdx, colTypes, IM_ARRAYSIZE(colTypes))) {
                        phys.colliderType = static_cast<ColliderType>(colIdx);
                        changed = true;
                    }

                    if (phys.colliderType == ColliderType::Box) {
                        changed |= ImGui::DragFloat3("Half Extents", &phys.boxHalfExtents.x, 0.1f);
                    } else if (phys.colliderType == ColliderType::Sphere) {
                        changed |= ImGui::DragFloat("Radius", &phys.radius, 0.1f);
                    } else if (phys.colliderType == ColliderType::Capsule) {
                        changed |= ImGui::DragFloat("Radius", &phys.radius, 0.1f);
                        changed |= ImGui::DragFloat("Height", &phys.height, 0.1f);
                    } else if (phys.colliderType == ColliderType::Mesh) {
                        changed |= ImGui::Checkbox("Use Attached Model", &phys.useModelMesh);
                        if (!phys.useModelMesh) {
                            ImGui::Text("Mesh Path: %s", phys.meshAssetPath.empty() ? "None" : phys.meshAssetPath.c_str());
                            ImGui::SameLine();
                            if (ImGui::Button("Load Proxy")) {
                                auto selection = pfd::open_file("Load Collision Mesh", ".", { "3D Models", "*.gltf *.glb *.obj" }).result();
                                if (!selection.empty()) {
                                    /* Todo: load proxy */
                                }
                            }
                        }
                        changed |= ImGui::Checkbox("Convex Hull", &phys.isConvex);
                    }

                    if (changed) {
                        auto* engine = m_selectedEntity.getScene().getEngineContext();
                        engine->physics().destroyRigidBody(m_selectedEntity);
                        engine->physics().createRigidBody(m_selectedEntity);
                    }
                }
            } else if (m_focusedComponent == "AudioSource" && m_selectedEntity.has<AudioSourceComponent>()) {
                if (ComponentPropertiesHeader("AudioSource", ICON_FA_VOLUME_HIGH, colAudio, true, [&](){ m_selectedEntity.remove<AudioSourceComponent>(); m_focusedComponent = ""; })) {
                    auto& audio = m_selectedEntity.get<AudioSourceComponent>();
                    ImGui::Text("File: %s", audio.assetPath.empty() ? "None" : audio.assetPath.c_str());
                    if (ImGui::Button(ICON_FA_FOLDER_OPEN " Load Audio")) {
                        auto selection = pfd::open_file("Load Audio", ".", { "Audio Files", "*.wav *.mp3 *.ogg *.flac" }).result();
                        if (!selection.empty()) {
                            audio.assetPath = selection[0];
                        }
                    }
                    ImGui::SliderFloat("Volume", &audio.volume, 0.0f, 2.0f);
                    ImGui::Checkbox("Looping", &audio.loop);
                }
            } else if (m_focusedComponent == "AudioListener" && m_selectedEntity.has<AudioListenerComponent>()) {
                if (ComponentPropertiesHeader("AudioListener", ICON_FA_EAR_LISTEN, colAudio, true, [&](){ m_selectedEntity.remove<AudioListenerComponent>(); m_focusedComponent = ""; })) {
                    auto& listener = m_selectedEntity.get<AudioListenerComponent>();
                    ImGui::Checkbox("Primary Active", &listener.active);
                }
            } else if (m_focusedComponent == "SimpleAnimation" && m_selectedEntity.has<SimpleAnimationComponent>()) {
                if (ComponentPropertiesHeader("SimpleAnimation", ICON_FA_FILM, colLogic, true, [&](){ m_selectedEntity.remove<SimpleAnimationComponent>(); m_focusedComponent = ""; })) {
                    auto& anim = m_selectedEntity.get<SimpleAnimationComponent>();
                    ImGui::Checkbox("Active", &anim.active);
                    
                    const char* types[] = { "Rotation", "Translation", "Waypoints" };
                    int currentType = static_cast<int>(anim.type);
                    if (ImGui::Combo("Type", &currentType, types, IM_ARRAYSIZE(types))) anim.type = static_cast<SimpleAnimationType>(currentType);
                    
                    ImGui::DragFloat("Speed", &anim.speed, 0.1f);
                    ImGui::Checkbox("Physics Sync", &anim.physicsSync);

                    if (anim.type == SimpleAnimationType::Rotation) {
                        ImGui::DragFloat3("Rotation Axis", &anim.rotationAxis.x, 0.01f);
                    } else if (anim.type == SimpleAnimationType::Translation) {
                        ImGui::DragFloat3("Direction", &anim.direction.x, 0.01f);
                        ImGui::DragFloat("Amplitude", &anim.amplitude, 0.1f);
                        ImGui::Checkbox("Ping Pong", &anim.pingPong);
                    } else if (anim.type == SimpleAnimationType::Waypoints) {
                        ImGui::Checkbox("Loop", &anim.loop);
                        if (ImGui::TreeNodeEx("Waypoints List", ImGuiTreeNodeFlags_DefaultOpen)) {
                            for (size_t i = 0; i < anim.waypoints.size(); i++) {
                                ImGui::PushID((int)i);
                                ImGui::DragFloat3("##Pos", &anim.waypoints[i].x, 0.1f);
                                ImGui::SameLine();
                                if (ImGui::Button(ICON_FA_TRASH)) {
                                    anim.waypoints.erase(anim.waypoints.begin() + i);
                                    ImGui::PopID();
                                    break;
                                }
                                ImGui::PopID();
                            }
                            if (ImGui::Button(ICON_FA_PLUS " Add Point")) {
                                glm::vec3 pos = m_selectedEntity.get<TransformComponent>().translation;
                                if (!anim.waypoints.empty()) pos = anim.waypoints.back();
                                anim.waypoints.push_back(pos);
                            }
                            ImGui::TreePop();
                        }
                    }
                }
            } else if (m_focusedComponent == "OrbitalGravity" && m_selectedEntity.has<OrbitalGravityComponent>()) {
                if (ComponentPropertiesHeader("OrbitalGravity", ICON_FA_MAGNET, colLogic, true, [&](){ m_selectedEntity.remove<OrbitalGravityComponent>(); m_focusedComponent = ""; })) {
                    auto& ogc = m_selectedEntity.get<OrbitalGravityComponent>();
                    ImGui::DragFloat("Strength (GM)", &ogc.strength, 5.0f);
                }
            } else if (m_focusedComponent == "SpaceshipController" && m_selectedEntity.has<SpaceshipControllerComponent>()) {
                if (ComponentPropertiesHeader("SpaceshipController", ICON_FA_ROCKET, colLogic, true, [&](){ m_selectedEntity.remove<SpaceshipControllerComponent>(); m_focusedComponent = ""; })) {
                    auto& sc = m_selectedEntity.get<SpaceshipControllerComponent>();
                    ImGui::DragFloat("Main Thrust", &sc.mainThrustPower, 5.0f);
                    ImGui::DragFloat("Retro Thrust", &sc.retroThrustPower, 2.0f);
                    ImGui::DragFloat("Torque (RCS)", &sc.torquePower, 1.0f);
                }
            }
        }

        ImGui::Separator();
        if (ImGui::Button(ICON_FA_PLUS " Add Component")) ImGui::OpenPopup("AddComponentPopup");
        if (ImGui::BeginPopup("AddComponentPopup")) {
            if (ImGui::MenuItem("Camera")) m_selectedEntity.add<CameraComponent>();
            if (ImGui::MenuItem("FPS Controller")) m_selectedEntity.add<FPSControllerComponent>();
            if (ImGui::MenuItem("Orbit Controller")) m_selectedEntity.add<OrbitControllerComponent>();
            if (ImGui::MenuItem("Light")) m_selectedEntity.add<LightComponent>();
            if (ImGui::MenuItem("Skybox")) m_selectedEntity.add<SkyboxComponent>();
            if (ImGui::MenuItem("SkySphere")) m_selectedEntity.add<SkySphereComponent>();
            if (ImGui::MenuItem("Physics Body")) {
                m_selectedEntity.add<PhysicsComponent>();
                m_selectedEntity.getScene().getEngineContext()->physics().createRigidBody(m_selectedEntity);
            }
            if (ImGui::MenuItem("Audio Source")) m_selectedEntity.add<AudioSourceComponent>();
            if (ImGui::MenuItem("Audio Listener")) m_selectedEntity.add<AudioListenerComponent>();
            if (ImGui::MenuItem("Simple Animation")) m_selectedEntity.add<SimpleAnimationComponent>();
            if (ImGui::MenuItem("Orbital Gravity")) m_selectedEntity.add<OrbitalGravityComponent>();
            if (ImGui::MenuItem("Spaceship Controller")) m_selectedEntity.add<SpaceshipControllerComponent>();
            ImGui::EndPopup();
        }
    } else { ImGui::TextDisabled("Select an entity to view its properties."); }
    ImGui::End();
}

void ImGuiLayer::showMainMenu() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            auto& engine = Engine::Get();
            if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " Save Scene")) {
                auto res = pfd::save_file("Save Scene", "scene.json", { "Scene Files", "*.json" }).result();
                if (!res.empty()) engine.exportScene(res);
            }
            if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Load Scene")) {
                auto res = pfd::open_file("Load Scene", ".", { "Scene Files", "*.json" }).result();
                if (!res.empty()) engine.importScene(res[0]);
            }
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_FA_XMARK " Exit")) engine.Stop();
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void ImGuiLayer::showToolbar() {
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 2));
    if (ImGui::Begin("Toolbar", nullptr, flags)) {
        ImGui::PopStyleVar();
        auto& engine = Engine::Get();
        bool paused = engine.isPhysicsPaused();
        float size = ImGui::CalcTextSize(ICON_FA_PLAY " Play "" " ICON_FA_ARROW_ROTATE_LEFT " Reset Scene ").x;
        float off = (ImGui::GetContentRegionAvail().x - size) * 0.5f;
        if (off > 0.0f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);
        if (paused) { if (ImGui::Button(ICON_FA_PLAY " Play")) engine.setPhysicsPaused(false); }
        else { if (ImGui::Button(ICON_FA_PAUSE " Pause")) engine.setPhysicsPaused(true); }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_ARROW_ROTATE_LEFT " Reset Scene")) { engine.resetScene(); engine.setPhysicsPaused(true); }
    } else { ImGui::PopStyleVar(); }
    ImGui::End();
}

} // namespace bb3d

#endif // BB3D_ENABLE_EDITOR
