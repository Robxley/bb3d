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
    initInfo.ImageCount = swapChain.getImageCount(); 
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

void ImGuiLayer::showViewport(RenderTarget* renderTarget, Scene& scene) {
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

        m_viewportFocused = ImGui::IsWindowFocused();
        m_viewportHovered = ImGui::IsWindowHovered();

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
        } else if (entity.has<RigidBodyComponent>() || entity.has<BoxColliderComponent>() || entity.has<NativeScriptComponent>()) {
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
                // Plane a déjà un material par défaut via checkerboard, on le laisse ou on le remplace si on veut éditer
                // MeshGenerator::createCheckerboardPlane ne met pas de material PBR standard mais utilise des couleurs vertex.
                // Pour l'éditer, on force un PBR blanc.
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

    // Use unique ID to prevent widget conflicts when multiple entities share names.
    ImGui::PushID((int)(entt::entity)entity);

    ImGuiTreeNodeFlags flags = ((m_selectedEntity == entity) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
    flags |= ImGuiTreeNodeFlags_SpanAvailWidth;
    flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    // Determinate Icon based on components
    const char* icon = ICON_FA_FILE; // Default fallback
    ImVec4 iconColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // Default white
    if (entity.has<CameraComponent>()) {
        if (entity.has<FPSControllerComponent>()) icon = ICON_FA_VIDEO;
        else if (entity.has<OrbitControllerComponent>()) icon = ICON_FA_GLOBE;
        else icon = ICON_FA_CAMERA;
        iconColor = ImVec4(0.3f, 0.8f, 1.0f, 1.0f); // Light blue
    } else if (entity.has<LightComponent>()) {
        auto type = entity.get<LightComponent>().type;
        if (type == LightType::Directional) icon = ICON_FA_SUN;
        else icon = ICON_FA_LIGHTBULB;
        iconColor = ImVec4(1.0f, 0.9f, 0.2f, 1.0f); // Yellow
    } else if (entity.has<TerrainComponent>()) {
        icon = ICON_FA_MOUNTAIN;
        iconColor = ImVec4(0.2f, 0.8f, 0.3f, 1.0f); // Forest green
    } else if (entity.has<ParticleSystemComponent>()) {
        icon = ICON_FA_WAND_MAGIC_SPARKLES;
        iconColor = ImVec4(1.0f, 0.4f, 0.8f, 1.0f); // Pink
    } else if (entity.has<SkyboxComponent>() || entity.has<SkySphereComponent>()) {
        icon = ICON_FA_CLOUD;
        iconColor = ImVec4(0.6f, 0.8f, 0.9f, 1.0f); // Sky blue
    } else if (entity.has<AudioListenerComponent>()) {
        icon = ICON_FA_HEADPHONES;
        iconColor = ImVec4(1.0f, 0.6f, 0.2f, 1.0f); // Orange
    } else if (entity.has<AudioSourceComponent>()) {
        icon = ICON_FA_VOLUME_HIGH;
        iconColor = ImVec4(1.0f, 0.6f, 0.2f, 1.0f); // Orange
    } else if (entity.has<ModelComponent>()) {
        icon = ICON_FA_CUBES;
        iconColor = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
    } else if (entity.has<MeshComponent>()) {
        auto type = entity.get<MeshComponent>().primitiveType;
        if (type == PrimitiveType::Cube) icon = ICON_FA_CUBE;
        else if (type == PrimitiveType::Sphere) icon = ICON_FA_CIRCLE;
        else if (type == PrimitiveType::Plane) icon = ICON_FA_SQUARE;
        else icon = ICON_FA_CUBE; // default mesh
        iconColor = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
    } else if (entity.has<NativeScriptComponent>()) {
        icon = ICON_FA_CODE;
        iconColor = ImVec4(0.6f, 0.4f, 1.0f, 1.0f); // Purple
    } else if (entity.has<RigidBodyComponent>()) {
        icon = ICON_FA_BOX;
        iconColor = ImVec4(0.4f, 1.0f, 0.4f, 1.0f); // Lime
    } else if (entity.has<BoxColliderComponent>() || entity.has<SphereColliderComponent>() || entity.has<CapsuleColliderComponent>() || entity.has<MeshColliderComponent>()) {
        icon = ICON_FA_SHIELD_HALVED;
        iconColor = ImVec4(0.4f, 1.0f, 0.4f, 1.0f); // Lime
    }

    // Draw the node without text so the Selection hitbox is consistent across the line
    ImGui::TreeNodeEx("##node", flags, "");
    
    if (ImGui::IsItemHovered()) {
        m_hoveredEntity = entity;
    }

    if (ImGui::IsItemClicked()) {
        m_selectedEntity = entity;
    }

    // Now overlay the colored icon and the text
    ImGui::SameLine();
    ImGui::TextColored(iconColor, "%s", icon);
    ImGui::SameLine();
    ImGui::Text("%s", tag.c_str());

    bool entityDeleted = false;
    if (ImGui::BeginPopupContextItem("EntityContextMenu")) {
        if (ImGui::MenuItem(ICON_FA_TRASH " Delete Entity"))
            entityDeleted = true;
        ImGui::EndPopup();
    }

    if (entityDeleted) {
        entity.getScene().destroyEntity(entity);
        if (m_selectedEntity == entity) m_selectedEntity = {};
    }

    ImGui::PopID();
}

void ImGuiLayer::showSceneSettings(Scene& scene) {
    ImGui::Begin(ICON_FA_SLIDERS " Scene Settings");

    if (ImGui::CollapsingHeader(ICON_FA_CAMERA " Camera Control", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button(ICON_FA_ARROWS_ROTATE " Reset Camera View")) {
            auto view = scene.getRegistry().view<CameraComponent>();
            for (auto entity : view) {
                Entity e(entity, scene);
                
                // Reset position de base
                if (e.has<TransformComponent>()) {
                    auto& tc = e.get<TransformComponent>();
                    tc.translation = glm::vec3(0, 2, 8);
                    tc.rotation = glm::vec3(0);
                }

                // Reset spécifique au contrôleur FPS
                if (e.has<FPSControllerComponent>()) {
                    auto& fps = e.get<FPSControllerComponent>();
                    fps.yaw = -90.0f;
                    fps.pitch = 0.0f;
                }

                // Reset spécifique au contrôleur Orbital
                if (e.has<OrbitControllerComponent>()) {
                    auto& orbit = e.get<OrbitControllerComponent>();
                    orbit.target = glm::vec3(0, 0, 0);
                    orbit.distance = 15.0f;
                    orbit.yaw = 45.0f;
                    orbit.pitch = 45.0f; 
                }
            }
            BB_CORE_INFO("Editor: Camera view reset to defaults.");
        }
    }

    ImGui::Separator();
    ImGui::TextDisabled("Future options:");
    static bool showGrid = true;
    ImGui::Checkbox("Show Grid", &showGrid);
    static bool showPhysics = false;
    ImGui::Checkbox("Debug Physics Colliders", &showPhysics);

    ImGui::End();
}

void ImGuiLayer::showInspector() {
    ImGui::Begin(ICON_FA_CIRCLE_INFO " Inspector");

    if (m_selectedEntity) {
        // --- TAG EDITOR ---
        auto& tag = m_selectedEntity.get<TagComponent>().tag;
        char buffer[256];
        memset(buffer, 0, sizeof(buffer));
        strcpy_s(buffer, tag.c_str());
        if (ImGui::InputText(ICON_FA_TAG " Tag", buffer, sizeof(buffer))) {
            tag = std::string(buffer);
        }

        ImGui::Separator();

        // --- TRANSFORM COMPONENT ---
        if (m_selectedEntity.has<TransformComponent>()) {
            if (ImGui::CollapsingHeader(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT " Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& tc = m_selectedEntity.get<TransformComponent>();
                bool changed = false;
                if (ImGui::DragFloat3("Position", &tc.translation.x, 0.1f)) changed = true;
                glm::vec3 rotationDeg = glm::degrees(tc.rotation);
                if (ImGui::DragFloat3("Rotation", &rotationDeg.x, 0.1f)) {
                    tc.rotation = glm::radians(rotationDeg);
                    changed = true;
                }
                if (ImGui::DragFloat3("Scale", &tc.scale.x, 0.1f)) changed = true;

                if (changed && m_selectedEntity.has<RigidBodyComponent>()) {
                    Engine::Get().physics().updateBodyTransform(m_selectedEntity);
                }

                ImGui::Spacing();
                if (ImGui::Button(ICON_FA_ARROW_ROTATE_LEFT " Reset to Initial")) {
                    tc.resetToInitial();
                    if (m_selectedEntity.has<RigidBodyComponent>()) {
                        Engine::Get().physics().resetBody(m_selectedEntity);
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save as Initial")) {
                    tc.saveInitialState();
                }
            }
        }

        // Helper to draw a material properties UI.
        auto DrawMaterialUI = [&](Material* material) {
            if (!material) return;
            if (ImGui::CollapsingHeader(ICON_FA_PALETTE " Material", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto pbrMat = dynamic_cast<PBRMaterial*>(material);
                if (pbrMat) {
                    glm::vec3 color = pbrMat->getColor();
                    if (ImGui::ColorEdit3("Albedo Color", &color.x)) {
                        pbrMat->setColor(color);
                        // Synchronisation avec MeshComponent pour l'export
                        if (m_selectedEntity.has<MeshComponent>()) {
                            m_selectedEntity.get<MeshComponent>().color = color;
                        }
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

                    if (ImGui::TreeNode("ORM Map Generator (Combine Maps)")) {
                        static std::string aoPath, roughPath, metalPath;
                        
                        auto PathSelector = [](const char* label, std::string& path) {
                            ImGui::PushID(label);
                            if (ImGui::Button(ICON_FA_FOLDER_OPEN)) {
                                auto res = pfd::open_file("Select Map", ".", { "Images", "*.jpg *.png *.tga" }).result();
                                if (!res.empty()) path = res[0];
                            }
                            ImGui::SameLine();
                            ImGui::InputText(label, path.data(), path.capacity() + 1, ImGuiInputTextFlags_ReadOnly);
                            ImGui::PopID();
                        };

                        PathSelector("AO (R)", aoPath);
                        PathSelector("Roughness (G)", roughPath);
                        PathSelector("Metallic (B)", metalPath);

                        if (ImGui::Button("Generate & Apply ORM")) {
                            if (!aoPath.empty() || !roughPath.empty() || !metalPath.empty()) {
                                auto tex = TextureGenerator::combineORM(m_context, aoPath, roughPath, metalPath);
                                if (tex) pbrMat->setORMMap(tex);
                            }
                        }
                        ImGui::TreePop();
                    }

                    if (ImGui::Button("Reset Material")) {
                        pbrMat->setColor({1.0f, 1.0f, 1.0f});
                        pbrMat->setAlbedoMap(nullptr);
                        pbrMat->setNormalMap(nullptr);
                        pbrMat->setORMMap(nullptr);
                        pbrMat->setEmissiveMap(nullptr);
                    }
                }
            }
        };

        // --- MESH COMPONENT ---
        if (m_selectedEntity.has<MeshComponent>()) {
            if (ImGui::CollapsingHeader(ICON_FA_CUBE " Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& mc = m_selectedEntity.get<MeshComponent>();
                ImGui::Checkbox("Visible", &mc.visible);
                ImGui::SameLine();
                if (ImGui::Button(ICON_FA_TRASH " Remove Component")) {
                    m_selectedEntity.remove<MeshComponent>();
                } else if (mc.mesh) {
                    DrawMaterialUI(mc.mesh->getMaterial().get());
                }
            }
        }

        // --- MODEL COMPONENT ---
        if (m_selectedEntity.has<ModelComponent>()) {
            if (ImGui::CollapsingHeader(ICON_FA_CUBES " Model", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& mc = m_selectedEntity.get<ModelComponent>();
                ImGui::Checkbox("Visible", &mc.visible);
                ImGui::SameLine();
                if (ImGui::Button(ICON_FA_TRASH " Remove Component")) {
                    m_selectedEntity.remove<ModelComponent>();
                } else if (mc.model) {
                    // --- NORMALIZATION TOOL ---
                    ImGui::SeparatorText(ICON_FA_MAXIMIZE " Normalization");
                    static float uniformSize = 1.0f;
                    ImGui::DragFloat("Target Uniform Size", &uniformSize, 0.1f, 0.01f, 100.0f);
                    if (ImGui::Button("Normalize (Uniform)")) {
                        mc.model->normalize(glm::vec3(uniformSize));
                        BB_CORE_INFO("Editor: Model normalized uniformly to size {}", uniformSize);
                    }
                    ImGui::TextDisabled("(Preserves proportions based on largest axis)");

                    ImGui::Separator();
                    if (ImGui::TreeNode("Materials")) {
                        int matIdx = 0;
                        for (auto& mesh : mc.model->getMeshes()) {
                            ImGui::PushID(matIdx++);
                            if (ImGui::TreeNode(mesh.get(), "Material %d", matIdx)) {
                                DrawMaterialUI(mesh->getMaterial().get());
                                ImGui::TreePop();
                            }
                            ImGui::PopID();
                        }
                        ImGui::TreePop();
                    }
                }
            }
        }

        // --- RIGIDBODY COMPONENT ---
        if (m_selectedEntity.has<RigidBodyComponent>()) {
            if (ImGui::CollapsingHeader(ICON_FA_WEIGHT_HANGING " RigidBody (Physics)", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& rb = m_selectedEntity.get<RigidBodyComponent>();
                
                const char* types[] = { "Static", "Dynamic", "Kinematic", "Character" };
                int currentType = static_cast<int>(rb.type);
                if (ImGui::Combo("Type", &currentType, types, IM_ARRAYSIZE(types))) {
                    rb.type = static_cast<BodyType>(currentType);
                }

                if (rb.type == BodyType::Dynamic) {
                    ImGui::DragFloat("Mass", &rb.mass, 0.1f, 0.01f, 1000.0f);
                }
                ImGui::SliderFloat("Friction", &rb.friction, 0.0f, 1.0f);
                ImGui::SliderFloat("Restitution", &rb.restitution, 0.0f, 1.0f);

                if (ImGui::Button(ICON_FA_TRASH " Remove Physics")) {
                    m_selectedEntity.remove<RigidBodyComponent>();
                    if (m_selectedEntity.has<BoxColliderComponent>()) m_selectedEntity.remove<BoxColliderComponent>();
                    if (m_selectedEntity.has<SphereColliderComponent>()) m_selectedEntity.remove<SphereColliderComponent>();
                    if (m_selectedEntity.has<MeshColliderComponent>()) m_selectedEntity.remove<MeshColliderComponent>();
                }
            }
        } else {
            ImGui::Separator();
            if (ImGui::Button(ICON_FA_PLUS " Add Physics (RigidBody)")) {
                m_selectedEntity.add<RigidBodyComponent>();
                auto& rb = m_selectedEntity.get<RigidBodyComponent>();
                rb.type = BodyType::Dynamic; // Par défaut dynamique

                // Deviner le collider
                if (tag.find("Cube") != std::string::npos || tag.find("Floor") != std::string::npos || tag.find("Plane") != std::string::npos) {
                    m_selectedEntity.add<BoxColliderComponent>();
                    if (tag.find("Floor") != std::string::npos || tag.find("Plane") != std::string::npos) {
                        rb.type = BodyType::Static; // Sol statique par défaut
                        auto& box = m_selectedEntity.get<BoxColliderComponent>();
                        // On utilise un collider très fin pour que le haut du sol (Y=0.005) 
                        // soit presque confondu avec le visuel (Y=0).
                        box.halfExtents = {10.0f, 0.005f, 10.0f}; 
                    }
                } else if (tag.find("Sphere") != std::string::npos) {
                    m_selectedEntity.add<SphereColliderComponent>();
                } else if (m_selectedEntity.has<MeshComponent>() || m_selectedEntity.has<ModelComponent>()) {
                    // Pour les meshs arbitraires
                    m_selectedEntity.add<MeshColliderComponent>();
                    auto& mc = m_selectedEntity.get<MeshColliderComponent>();
                    if (m_selectedEntity.has<MeshComponent>()) {
                        auto& meshComp = m_selectedEntity.get<MeshComponent>();
                        mc.mesh = meshComp.mesh;
                        mc.assetPath = meshComp.assetPath;
                    } else {
                        auto& modelComp = m_selectedEntity.get<ModelComponent>();
                        if (modelComp.model && !modelComp.model->getMeshes().empty()) {
                            mc.mesh = modelComp.model->getMeshes()[0];
                            mc.assetPath = modelComp.assetPath;
                        }
                    }
                    rb.type = BodyType::Static; // Mesh collider est souvent statique (pour le décor)
                }
            }
        }

        ImGui::Separator();
        
        // --- ADD/REPLACE ASSET ACTION ---
        if (ImGui::Button(ICON_FA_FOLDER_OPEN " Load Asset onto Entity...")) {
            auto selection = pfd::open_file("Select Asset", ".", { "3D Models", "*.gltf *.glb *.obj" }).result();
            if (!selection.empty()) {
                try {
                    std::string path = selection[0];
                    if (m_selectedEntity.has<MeshComponent>()) m_selectedEntity.remove<MeshComponent>();
                    if (m_selectedEntity.has<ModelComponent>()) m_selectedEntity.remove<ModelComponent>();
                    
                    auto model = Engine::Get().assets().load<Model>(path);
                    
                    // Centrage automatique : On centre les maillages et on déplace l'entité
                    // Cela permet d'avoir l'origine physique au centre visuel de l'objet.
                    glm::vec3 center = model->normalize(model->getBounds().size());
                    m_selectedEntity.get<TransformComponent>().translation += center;

                    m_selectedEntity.add<ModelComponent>(model, path);
                } catch (const std::exception& e) {
                    BB_CORE_ERROR("Editor: Failed to load asset: {}", e.what());
                }
            }
        }
    } else {
        ImGui::TextDisabled("No entity selected in hierarchy.");
    }

        ImGui::End();

    }

    

    void ImGuiLayer::showMainMenu() {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                auto& engine = Engine::Get();
                if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " Save Scene", "Ctrl+S")) {
                    auto res = pfd::save_file("Save Scene", "scene.json", { "Scene Files", "*.json" }).result();
                    if (!res.empty()) {
                        engine.exportScene(res);
                    }
                }
                if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Load Scene", "Ctrl+O")) {
                    auto res = pfd::open_file("Load Scene", ".", { "Scene Files", "*.json" }).result();
                    if (!res.empty()) {
                        engine.importScene(res[0]);
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_FA_XMARK " Exit", "Alt+F4")) {
                    engine.Stop();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
    }
    
void ImGuiLayer::showToolbar() {
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
    
    if (ImGui::Begin("Toolbar", nullptr, flags)) {
        ImGui::PopStyleVar(2);
        
        auto& engine = Engine::Get();
        bool paused = engine.isPhysicsPaused();

        // Calculate center position to align buttons in the middle
        float size = ImGui::CalcTextSize(ICON_FA_PLAY " Play "" " ICON_FA_ARROW_ROTATE_LEFT " Reset Scene ").x;
        float avail = ImGui::GetContentRegionAvail().x;
        float off = (avail - size) * 0.5f;
        if (off > 0.0f) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);
        }

        if (paused) {
            if (ImGui::Button(ICON_FA_PLAY " Play")) {
                engine.setPhysicsPaused(false);
            }
        } else {
            if (ImGui::Button(ICON_FA_PAUSE " Pause")) {
                engine.setPhysicsPaused(true);
            }
        }

                        ImGui::SameLine();
                        if (ImGui::Button(ICON_FA_ARROW_ROTATE_LEFT " Reset Scene")) {
                            engine.resetScene();
                            engine.setPhysicsPaused(true);
                        }
                    } else {
                        ImGui::PopStyleVar(2);
                    }
                    ImGui::End();
                }

} // namespace bb3d

#endif // BB3D_ENABLE_EDITOR