#include "bb3d/core/ImGuiLayer.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/core/Window.hpp"
#include "bb3d/core/Log.hpp"
#include "bb3d/core/IconsFontAwesome6.h"
#include "bb3d/scene/Scene.hpp"
#include "bb3d/core/portable-file-dialogs.h"
#include "bb3d/core/Engine.hpp"
#include "bb3d/render/MeshGenerator.hpp"
#include "bb3d/render/TextureGenerator.hpp"

#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <imgui_freetype.h>

#include <SDL3/SDL.h>
#include <filesystem>

namespace bb3d {

ImGuiLayer::ImGuiLayer(VulkanContext& context, Window& window)
    : m_context(context), m_window(window) {
    
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    
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
    initInfo.ImageCount = 3; 
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
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::endFrame(vk::CommandBuffer commandBuffer) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), static_cast<VkCommandBuffer>(commandBuffer));
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

void ImGuiLayer::showSceneHierarchy(Scene& scene) {
    ImGui::Begin(ICON_FA_SITEMAP " Hierarchy");

    // Iterate through all entities in the registry.
    scene.getRegistry().view<entt::entity>().each([&](auto entityHandle) {
        Entity entity(entityHandle, scene);
        drawEntityNode(entity);
    });

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
                e.add<MeshComponent>(MeshGenerator::createCube(m_context, 1.0f));
                assignDefaultMat(e);
            }
            if (ImGui::MenuItem("Sphere")) {
                auto e = scene.createEntity("Sphere " + std::to_string(entityCount++));
                e.add<MeshComponent>(MeshGenerator::createSphere(m_context, 0.5f, 32));
                assignDefaultMat(e);
            }
            if (ImGui::MenuItem("Plane")) {
                auto e = scene.createEntity("Plane " + std::to_string(entityCount++));
                e.add<MeshComponent>(MeshGenerator::createCheckerboardPlane(m_context, 10.0f, 10));
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

    ImGui::TreeNodeEx("##node", flags, "%s %s", ICON_FA_CUBE, tag.c_str());
    
    if (ImGui::IsItemClicked()) {
        m_selectedEntity = entity;
    }

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
                ImGui::DragFloat3("Position", &tc.translation.x, 0.1f);
                glm::vec3 rotationDeg = glm::degrees(tc.rotation);
                if (ImGui::DragFloat3("Rotation", &rotationDeg.x, 0.1f)) {
                    tc.rotation = glm::radians(rotationDeg);
                }
                ImGui::DragFloat3("Scale", &tc.scale.x, 0.1f);
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
                    }

                    auto TextureSlot = [&](const char* label, std::function<void(Ref<Texture>)> setter) {
                        ImGui::PushID(label);
                        if (ImGui::Button(ICON_FA_FOLDER_OPEN)) {
                            auto selection = pfd::open_file("Load Texture", ".", { "Image Files", "*.png *.jpg *.jpeg *.tga *.bmp" }).result();
                            if (!selection.empty()) {
                                try {
                                    auto tex = Engine::Get().assets().load<Texture>(selection[0]);
                                    setter(tex);
                                } catch (const std::exception& e) { BB_CORE_ERROR("Failed to load texture: {}", e.what()); }
                            }
                        }
                        ImGui::SameLine(); ImGui::Text("%s", label);
                        ImGui::PopID();
                    };

                    TextureSlot("Albedo Map", [&](Ref<Texture> t){ pbrMat->setAlbedoMap(t); });
                    TextureSlot("Normal Map", [&](Ref<Texture> t){ pbrMat->setNormalMap(t); });
                    TextureSlot("ORM Map", [&](Ref<Texture> t){ pbrMat->setORMMap(t); });
                    TextureSlot("Emissive Map", [&](Ref<Texture> t){ pbrMat->setEmissiveMap(t); });

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
                        box.halfExtents = {10.0f, 0.1f, 10.0f}; // Taille par défaut pour le sol
                    }
                } else if (tag.find("Sphere") != std::string::npos) {
                    m_selectedEntity.add<SphereColliderComponent>();
                } else if (m_selectedEntity.has<MeshComponent>()) {
                    // Pour les meshs arbitraires
                    m_selectedEntity.add<MeshColliderComponent>();
                    m_selectedEntity.get<MeshColliderComponent>().mesh = m_selectedEntity.get<MeshComponent>().mesh;
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

} // namespace bb3d