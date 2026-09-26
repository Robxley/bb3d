#include "bb3d/editor/panels/SceneHierarchyPanel.hpp"

#if defined(BB3D_ENABLE_EDITOR)

#include "bb3d/editor/EditorContext.hpp"
#include "bb3d/scene/Scene.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/core/IconsFontAwesome6.h"
#include "bb3d/core/Log.hpp"
#include "bb3d/core/Engine.hpp"
#include "bb3d/render/MeshGenerator.hpp"
#include "bb3d/render/Material.hpp"
#include "bb3d/core/portable-file-dialogs.h"

#include <imgui.h>
#include <filesystem>

namespace bb3d::editor {

void SceneHierarchyPanel::onImGuiRender() {
    if (!m_context) return;
    Scene* scene = m_context->getActiveScene();
    if (!scene) return;

    ImGui::Begin(ICON_FA_SITEMAP " Hierarchy", &m_isOpen);

    // Reset hovered entity at start of frame
    m_context->setHoveredEntity({});

    // Categorization vectors
    std::vector<Entity> cameras;
    std::vector<Entity> environment;
    std::vector<Entity> renderables;
    std::vector<Entity> audio;
    std::vector<Entity> logicAndPhysics;
    std::vector<Entity> other;

    scene->getRegistry().view<entt::entity>().each([&](auto entityHandle) {
        Entity entity(entityHandle, *scene);

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

        ImGui::PushID(title);
        ImGuiTreeNodeFlags headerFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
        bool open = ImGui::CollapsingHeader((std::string(icon) + " " + title).c_str(), headerFlags);
        if (open) {
            for (auto e : entities) {
                drawEntityNode(e);
            }
        }
        ImGui::PopID();
    };

    drawCategory("Cameras", ICON_FA_VIDEO, cameras);
    drawCategory("Environment", ICON_FA_SUN, environment);
    drawCategory("3D Objects", ICON_FA_CUBES, renderables);
    drawCategory("Audio", ICON_FA_MUSIC, audio);
    drawCategory("Physics & Logic", ICON_FA_BOLT, logicAndPhysics);
    drawCategory("Other", ICON_FA_FILE, other);

    // Deselect if clicking on empty space
    if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered()) {
        m_context->clearSelection();
    }

    // Context menu for entity creation
    if (ImGui::BeginPopupContextWindow(0, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::MenuItem(ICON_FA_PLUS " Create Empty Entity")) {
            scene->createEntity("Entity " + std::to_string(m_entityCount++));
        }

        if (ImGui::MenuItem(ICON_FA_FILE_IMPORT " Import 3D Model...")) {
            auto selection = pfd::open_file("Import 3D Model", ".", { "3D Models", "*.gltf *.glb *.obj" }).result();
            if (!selection.empty()) {
                std::filesystem::path path(selection[0]);
                std::string name = path.stem().string();
                scene->createModelEntity(name, selection[0], {0,0,0}, {1.0f, 1.0f, 1.0f});
                BB_CORE_INFO("Editor: Imported model {0} from {1}", name, selection[0]);
            }
        }

        if (ImGui::BeginMenu(ICON_FA_SHAPES " Create Primitive")) {
            auto* engine = m_context->getEngine();
            auto assignDefaultMat = [&](Entity e) {
                if (e.has<MeshComponent>() && engine) {
                    auto mesh = e.get<MeshComponent>().mesh;
                    if (mesh && !mesh->getMaterial()) {
                        mesh->setMaterial(CreateRef<PBRMaterial>(engine->GetVulkanContext()));
                    }
                }
            };

            if (engine) {
                if (ImGui::MenuItem("Cube")) {
                    auto e = scene->createEntity("Cube " + std::to_string(m_entityCount++));
                    e.add<MeshComponent>(MeshGenerator::createCube(engine->GetVulkanContext(), 1.0f), "", PrimitiveType::Cube);
                    assignDefaultMat(e);
                }
                if (ImGui::MenuItem("Sphere")) {
                    auto e = scene->createEntity("Sphere " + std::to_string(m_entityCount++));
                    e.add<MeshComponent>(MeshGenerator::createSphere(engine->GetVulkanContext(), 0.5f, 32), "", PrimitiveType::Sphere);
                    assignDefaultMat(e);
                }
                if (ImGui::MenuItem("Plane")) {
                    auto e = scene->createEntity("Plane " + std::to_string(m_entityCount++));
                    e.add<MeshComponent>(MeshGenerator::createCheckerboardPlane(engine->GetVulkanContext(), 10.0f, 10), "", PrimitiveType::Plane);
                    auto mesh = e.get<MeshComponent>().mesh;
                    mesh->setMaterial(CreateRef<PBRMaterial>(engine->GetVulkanContext()));
                }
            }
            ImGui::EndMenu();
        }

        ImGui::EndPopup();
    }

    ImGui::End();
}

void SceneHierarchyPanel::drawComponentList(Entity entity) {
    auto drawCompNode = [&](const char* name, const char* compIcon, ImVec4 color, bool hasComp) {
        if (!hasComp) return;
        ImGuiTreeNodeFlags compFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (m_context->getSelectedEntity() == entity && m_focusedComponent == name) {
            compFlags |= ImGuiTreeNodeFlags_Selected;
        }

        ImGui::PushID((void*)name);
        ImGui::TreeNodeEx("##CompNode", compFlags, "");
        if (ImGui::IsItemClicked()) {
            m_context->selectEntity(entity);
            m_focusedComponent = name;
        }

        ImGui::SameLine();
        ImGui::TextColored(color, "%s", compIcon);
        ImGui::SameLine();
        ImGui::Text("%s", name);
        ImGui::PopID();
    };

    ImVec4 colTransform = ImVec4(0.3f, 0.8f, 1.0f, 1.0f);
    ImVec4 colRender    = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
    ImVec4 colPhysics   = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
    ImVec4 colLight     = ImVec4(1.0f, 0.9f, 0.2f, 1.0f);
    ImVec4 colAudio     = ImVec4(1.0f, 0.6f, 0.2f, 1.0f);
    ImVec4 colLogic     = ImVec4(0.6f, 0.4f, 1.0f, 1.0f);

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
    drawCompNode("PointGravitySource", ICON_FA_MAGNET, colLogic, entity.has<PointGravitySourceComponent>());
    drawCompNode("SpaceshipController", ICON_FA_ROCKET, colLogic, entity.has<SpaceshipControllerComponent>());
    drawCompNode("SmartCamera", ICON_FA_VIDEO, colLogic, entity.has<SmartCameraComponent>());
    drawCompNode("ParticleSystem", ICON_FA_FIRE, colRender, entity.has<ParticleSystemComponent>());
    drawCompNode("Selectable", ICON_FA_ARROW_POINTER, colLogic, entity.has<SelectableComponent>());
}

void SceneHierarchyPanel::drawEntityNode(Entity entity) {
    auto& tag = entity.get<TagComponent>().tag;
    ImGui::PushID((int)(entt::entity)entity);

    bool isSelected = m_context->isSelected(entity);
    ImGuiTreeNodeFlags flags = ((isSelected && m_focusedComponent.empty()) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
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
    if (ImGui::IsItemHovered()) m_context->setHoveredEntity(entity);
    if (ImGui::IsItemClicked()) {
        bool ctrl = ImGui::GetIO().KeyCtrl;
        m_context->selectEntity(entity, ctrl);
        m_focusedComponent = "";
    }

    ImGui::SameLine();
    ImGui::TextColored(iconColor, "%s", icon);
    ImGui::SameLine();
    ImGui::Text("%s", tag.c_str());

    if (node_open) {
        drawComponentList(entity);
        ImGui::TreePop();
    }

    if (ImGui::BeginPopupContextItem("EntityContextMenu")) {
        if (ImGui::MenuItem(ICON_FA_TRASH " Delete Entity")) {
            entity.getScene().destroyEntity(entity);
            if (m_context->isSelected(entity)) {
                m_context->deselectEntity(entity);
            }
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();
}

} // namespace bb3d::editor

#endif // BB3D_ENABLE_EDITOR
