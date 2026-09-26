#include "bb3d/editor/panels/InspectorPanel.hpp"

#if defined(BB3D_ENABLE_EDITOR)

#include "bb3d/editor/EditorContext.hpp"
#include "bb3d/editor/EditorStyle.hpp"
#include "bb3d/scene/Scene.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/core/IconsFontAwesome6.h"
#include "bb3d/core/Log.hpp"
#include "bb3d/core/Engine.hpp"
#include "bb3d/render/Material.hpp"
#include "bb3d/render/Texture.hpp"
#include "bb3d/physics/PhysicsWorld.hpp"
#include "bb3d/core/portable-file-dialogs.h"

#include <imgui.h>
#include <glm/gtc/quaternion.hpp>
#include <cstring>

namespace bb3d::editor {

void InspectorPanel::onImGuiRender() {
    if (!m_context) return;

    ImGui::Begin(ICON_FA_CIRCLE_INFO " Inspector", &m_isOpen);

    Entity selected = m_context->getSelectedEntity();
    if (selected) {
        auto& tag = selected.get<TagComponent>().tag;

        // --- Section 1: Mini-Hierarchy (Structure) ---
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNodeEx("Structure", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth)) {
            ImGuiTreeNodeFlags entityFlags = (m_focusedComponent.empty() ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
            ImGui::TreeNodeEx((tag + "##InspectorTree").c_str(), entityFlags | ImGuiTreeNodeFlags_NoTreePushOnOpen, ICON_FA_CUBE " %s", tag.c_str());
            if (ImGui::IsItemClicked()) m_focusedComponent = "";

            drawComponentList(selected);
            ImGui::TreePop();
        }

        ImGui::Separator();

        // --- Section 2: Property Editor ---
        if (m_focusedComponent.empty()) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Entity Summary");
            char buffer[256];
            std::memset(buffer, 0, sizeof(buffer));
#if defined(_MSC_VER)
            strcpy_s(buffer, tag.c_str());
#else
            std::strncpy(buffer, tag.c_str(), sizeof(buffer) - 1);
#endif
            if (ImGui::InputText(ICON_FA_TAG " Tag", buffer, sizeof(buffer))) {
                tag = std::string(buffer);
            }
        } else {
            ImVec4 colTransform = ImVec4(0.3f, 0.8f, 1.0f, 1.0f);
            ImVec4 colRender    = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
            ImVec4 colPhysics   = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
            ImVec4 colLight     = ImVec4(1.0f, 0.9f, 0.2f, 1.0f);
            ImVec4 colAudio     = ImVec4(1.0f, 0.6f, 0.2f, 1.0f);
            ImVec4 colLogic     = ImVec4(0.6f, 0.4f, 1.0f, 1.0f);

            if (m_focusedComponent == "Transform" && selected.has<TransformComponent>()) {
                if (drawComponentHeader("Transform", ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT, colTransform, false)) {
                    auto& tc = selected.get<TransformComponent>();
                    bool tcChanged = false;

                    // Modern Ergonomic 3-Axis DragFloat Controls (Dark Pro Design System)
                    tcChanged |= EditorStyle::DrawVec3Control("Position", tc.translation, 0.0f);

                    glm::vec3 rotDeg = glm::degrees(tc.rotation);
                    if (EditorStyle::DrawVec3Control("Rotation", rotDeg, 0.0f)) {
                        tc.rotation = glm::radians(rotDeg);
                        tcChanged = true;
                    }

                    tcChanged |= EditorStyle::DrawVec3Control("Scale", tc.scale, 1.0f);

                    if (tcChanged && selected.has<PhysicsComponent>()) {
                        auto* ph = m_context->getEngine() ? m_context->getEngine()->GetPhysicsWorld() : nullptr;
                        if (ph) ph->updateBodyTransform(selected);
                    }

                    ImGui::Spacing();
                    if (ImGui::Button(ICON_FA_ARROW_ROTATE_LEFT " Reset")) {
                        tc.resetToInitial();
                        auto* ph = m_context->getEngine() ? m_context->getEngine()->GetPhysicsWorld() : nullptr;
                        if (ph) ph->resetBody(selected);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save Initial")) {
                        tc.saveInitialState();
                    }
                }
            } else if (m_focusedComponent == "Camera" && selected.has<CameraComponent>()) {
                if (drawComponentHeader("Camera", ICON_FA_VIDEO, colTransform, true, [&](){ selected.remove<CameraComponent>(); m_focusedComponent = ""; })) {
                    auto& cam = selected.get<CameraComponent>();
                    bool changed = false;
                    ImGui::Checkbox("Primary Active", &cam.active);
                    changed |= ImGui::DragFloat("FOV", &cam.fov, 0.5f, 10.0f, 120.0f);
                    changed |= ImGui::DragFloat("Near", &cam.nearPlane, 0.01f, 0.001f, 10.0f);
                    changed |= ImGui::DragFloat("Far", &cam.farPlane, 10.0f, 100.0f, 10000.0f);
                    if (changed && cam.camera) {
                        cam.camera->setPerspective(cam.fov, cam.aspect, cam.nearPlane, cam.farPlane);
                    }
                }
            } else if (m_focusedComponent == "OrbitController" && selected.has<OrbitControllerComponent>()) {
                if (drawComponentHeader("OrbitController", ICON_FA_CROSSHAIRS, colTransform, true, [&](){ selected.remove<OrbitControllerComponent>(); m_focusedComponent = ""; })) {
                    auto& oc = selected.get<OrbitControllerComponent>();
                    ImGui::DragFloat3("Target", &oc.target.x, 0.1f);
                    ImGui::DragFloat("Distance", &oc.distance, 0.1f, oc.minDistance, oc.maxDistance);
                    ImGui::DragFloat("Min Distance", &oc.minDistance, 0.1f, 0.1f, oc.maxDistance);
                    ImGui::DragFloat("Max Distance", &oc.maxDistance, 0.1f, oc.minDistance, 1000.0f);
                }
            } else if (m_focusedComponent == "FPSController" && selected.has<FPSControllerComponent>()) {
                if (drawComponentHeader("FPSController", ICON_FA_GAMEPAD, colTransform, true, [&](){ selected.remove<FPSControllerComponent>(); m_focusedComponent = ""; })) {
                    auto& fps = selected.get<FPSControllerComponent>();
                    ImGui::DragFloat3("Movement Speed", &fps.movementSpeed.x, 0.1f);
                    ImGui::DragFloat2("Rotation Speed", &fps.rotationSpeed.x, 0.01f);
                    ImGui::DragFloat("Yaw", &fps.yaw, 1.0f);
                    ImGui::DragFloat("Pitch", &fps.pitch, 1.0f, -89.0f, 89.0f);
                }
            } else if (m_focusedComponent == "Mesh" && selected.has<MeshComponent>()) {
                if (drawComponentHeader("Mesh", ICON_FA_CUBE, colRender)) {
                    auto& mc = selected.get<MeshComponent>();
                    ImGui::Checkbox("Visible", &mc.visible);
                    ImGui::Text("Primitive: %s", mc.assetPath.c_str());
                    if (mc.mesh) drawMaterialUI(mc.mesh->getMaterial().get());
                }
            } else if (m_focusedComponent == "Model" && selected.has<ModelComponent>()) {
                if (drawComponentHeader("Model", ICON_FA_CUBES, colRender)) {
                    auto& mc = selected.get<ModelComponent>();
                    ImGui::Checkbox("Visible", &mc.visible);

                    // Fix N7: Uses m_modelLoadConfig member instead of static s_loadConfig
                    const char* presets[] = { "Standard PBR", "Cell Shading" };
                    int currentPreset = static_cast<int>(m_modelLoadConfig.preset);
                    if (ImGui::Combo("Loading Preset", &currentPreset, presets, IM_ARRAYSIZE(presets))) {
                        m_modelLoadConfig.preset = static_cast<ModelLoadPreset>(currentPreset);
                    }

                    if (ImGui::TreeNode("Advanced Loading Options")) {
                        ImGui::Checkbox("Load Animations", &m_modelLoadConfig.loadAnimations);
                        ImGui::Checkbox("Load Materials", &m_modelLoadConfig.loadMaterials);
                        ImGui::Checkbox("Load PBR Maps", &m_modelLoadConfig.loadPBRMaps);
                        ImGui::Checkbox("Load Alpha Modes", &m_modelLoadConfig.loadAlphaModes);
                        ImGui::Checkbox("Load Vertex Colors", &m_modelLoadConfig.loadVertexColors);
                        ImGui::DragFloat3("Initial Scale", &m_modelLoadConfig.initialScale.x, 0.1f);
                        ImGui::TreePop();
                    }

                    ImGui::Text("File: %s", mc.assetPath.c_str());
                    ImGui::SameLine();
                    if (ImGui::Button(ICON_FA_FOLDER_OPEN "##Load")) {
                        auto selection = pfd::open_file("Select Model", ".", { "3D Models", "*.gltf *.glb *.obj" }).result();
                        if (!selection.empty() && m_context->getEngine()) {
                            mc.model = m_context->getEngine()->assets().load<Model>(selection[0], m_modelLoadConfig);
                            mc.assetPath = selection[0];
                            mc.model->normalize(glm::vec3(1.0f));
                            BB_CORE_INFO("Editor: Swapped model to {0} with custom config", selection[0]);
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(ICON_FA_ROTATE "##Reload") && !mc.assetPath.empty() && m_context->getEngine()) {
                        mc.model = m_context->getEngine()->assets().load<Model>(mc.assetPath, m_modelLoadConfig);
                        mc.model->normalize(glm::vec3(1.0f));
                        BB_CORE_INFO("Editor: Reloaded model {0} with custom config", mc.assetPath);
                    }

                    if (mc.model && ImGui::TreeNodeEx("Materials", ImGuiTreeNodeFlags_DefaultOpen)) {
                        int matIdx = 0;
                        for (auto& mesh : mc.model->getMeshes()) {
                            ImGui::PushID(matIdx++);
                            drawMaterialUI(mesh->getMaterial().get());
                            ImGui::PopID();
                        }
                        ImGui::TreePop();
                    }
                }
            // Fix N5: Duplicate empty 'Light' branch removed completely!
            } else if (m_focusedComponent == "Light" && selected.has<LightComponent>()) {
                if (drawComponentHeader("Light", ICON_FA_LIGHTBULB, colLight, true, [&](){ selected.remove<LightComponent>(); m_focusedComponent = ""; })) {
                    auto& light = selected.get<LightComponent>();
                    const char* types[] = { "Directional", "Point", "Spot" };
                    int currentType = static_cast<int>(light.type);
                    if (ImGui::Combo("Type", &currentType, types, IM_ARRAYSIZE(types))) light.type = static_cast<LightType>(currentType);
                    ImGui::ColorEdit3("Color", &light.color.x);
                    ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 100.0f);
                    if (light.type != LightType::Directional) ImGui::DragFloat("Range", &light.range, 0.1f, 0.0f, 500.0f);
                    ImGui::Checkbox("Cast Shadows", &light.castShadows);
                }
            } else if (m_focusedComponent == "Skybox" && selected.has<SkyboxComponent>()) {
                if (drawComponentHeader("Skybox", ICON_FA_CLOUD, colLight, true, [&](){ selected.remove<SkyboxComponent>(); m_focusedComponent = ""; })) {
                    auto& sky = selected.get<SkyboxComponent>();
                    ImGui::Text("File: %s", sky.assetPath.empty() ? "None" : sky.assetPath.c_str());
                    if (ImGui::Button(ICON_FA_FOLDER_OPEN " Load Cubemap/HDRI") && m_context->getEngine()) {
                        auto selection = pfd::open_file("Load Image", ".", { "Images", "*.hdr *.png *.jpg" }).result();
                        if (!selection.empty()) {
                            try {
                                sky.cubemap = m_context->getEngine()->assets().load<Texture>(selection[0], true);
                                sky.assetPath = selection[0];
                            } catch(...) {}
                        }
                    }
                }
            } else if (m_focusedComponent == "SkySphere" && selected.has<SkySphereComponent>()) {
                if (drawComponentHeader("SkySphere", ICON_FA_GLOBE, colLight, true, [&](){ selected.remove<SkySphereComponent>(); m_focusedComponent = ""; })) {
                    auto& sky = selected.get<SkySphereComponent>();
                    ImGui::Text("File: %s", sky.assetPath.empty() ? "None" : sky.assetPath.c_str());
                    if (ImGui::Button(ICON_FA_FOLDER_OPEN " Load Sphere Map") && m_context->getEngine()) {
                        auto selection = pfd::open_file("Load Image", ".", { "Images", "*.hdr *.png *.jpg" }).result();
                        if (!selection.empty()) {
                            try {
                                sky.texture = m_context->getEngine()->assets().load<Texture>(selection[0], true);
                                sky.assetPath = selection[0];
                            } catch(...) {}
                        }
                    }
                    ImGui::Checkbox("Flip Y-Axis", &sky.flipY);
                }
            } else if (m_focusedComponent == "PhysicsBody" && selected.has<PhysicsComponent>()) {
                if (drawComponentHeader("PhysicsBody", ICON_FA_BOX, colPhysics, true, [&](){
                    if (m_context->getEngine()) {
                        m_context->getEngine()->GetPhysicsWorld()->destroyRigidBody(selected);
                    }
                    selected.remove<PhysicsComponent>();
                    m_focusedComponent = "";
                })) {
                    auto& phys = selected.get<PhysicsComponent>();
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
                    changed |= ImGui::SliderFloat("Linear Damping", &phys.linearDamping, 0.0f, 10.0f);
                    changed |= ImGui::SliderFloat("Angular Damping", &phys.angularDamping, 0.0f, 10.0f);
                    changed |= ImGui::Checkbox("Constrain X-Y Plane", &phys.constrain2D);
                    changed |= ImGui::DragFloat("Collision Margin (Jolt)", &phys.collisionMargin, 0.005f, 0.0f, 0.5f, "%.3f");

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
                        }
                        changed |= ImGui::Checkbox("Convex Hull", &phys.isConvex);
                    }

                    if (changed && m_context->getEngine()) {
                        auto* ph = m_context->getEngine()->GetPhysicsWorld();
                        if (ph) {
                            ph->destroyRigidBody(selected);
                            ph->createRigidBody(selected);
                        }
                    }
                }
            } else if (m_focusedComponent == "AudioSource" && selected.has<AudioSourceComponent>()) {
                if (drawComponentHeader("AudioSource", ICON_FA_VOLUME_HIGH, colAudio, true, [&](){ selected.remove<AudioSourceComponent>(); m_focusedComponent = ""; })) {
                    auto& audio = selected.get<AudioSourceComponent>();
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
            } else if (m_focusedComponent == "AudioListener" && selected.has<AudioListenerComponent>()) {
                if (drawComponentHeader("AudioListener", ICON_FA_EAR_LISTEN, colAudio, true, [&](){ selected.remove<AudioListenerComponent>(); m_focusedComponent = ""; })) {
                    auto& listener = selected.get<AudioListenerComponent>();
                    ImGui::Checkbox("Primary Active", &listener.active);
                }
            } else if (m_focusedComponent == "SimpleAnimation" && selected.has<SimpleAnimationComponent>()) {
                if (drawComponentHeader("SimpleAnimation", ICON_FA_FILM, colLogic, true, [&](){ selected.remove<SimpleAnimationComponent>(); m_focusedComponent = ""; })) {
                    auto& anim = selected.get<SimpleAnimationComponent>();
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
                                ImGui::PushID(static_cast<int>(i));
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
                                glm::vec3 pos = selected.get<TransformComponent>().translation;
                                if (!anim.waypoints.empty()) pos = anim.waypoints.back();
                                anim.waypoints.push_back(pos);
                            }
                            ImGui::TreePop();
                        }
                    }
                }
            } else if (m_focusedComponent == "PointGravitySource" && selected.has<PointGravitySourceComponent>()) {
                if (drawComponentHeader("PointGravitySource", ICON_FA_MAGNET, colLogic, true, [&](){ selected.remove<PointGravitySourceComponent>(); m_focusedComponent = ""; })) {
                    auto& pgsc = selected.get<PointGravitySourceComponent>();
                    ImGui::DragFloat("Strength (GM)", &pgsc.strength, 5.0f);
                }
            } else if (m_focusedComponent == "SpaceshipController" && selected.has<SpaceshipControllerComponent>()) {
                if (drawComponentHeader("SpaceshipController", ICON_FA_ROCKET, colLogic, true, [&](){ selected.remove<SpaceshipControllerComponent>(); m_focusedComponent = ""; })) {
                    auto& sc = selected.get<SpaceshipControllerComponent>();
                    ImGui::DragFloat("Main Thrust", &sc.mainThrustPower, 5.0f);
                    ImGui::DragFloat("Retro Thrust", &sc.retroThrustPower, 2.0f);
                    ImGui::DragFloat("Torque (RCS)", &sc.torquePower, 1.0f);
                }
            } else if (m_focusedComponent == "SmartCamera" && selected.has<SmartCameraComponent>()) {
                if (drawComponentHeader("SmartCamera", ICON_FA_VIDEO, colLogic, true, [&](){ selected.remove<SmartCameraComponent>(); m_focusedComponent = ""; })) {
                    auto& scm = selected.get<SmartCameraComponent>();
                    ImGui::DragFloat("Min Zoom", &scm.minZoom, 1.0f, -50.0f, 500.0f);
                    ImGui::DragFloat("Max Zoom", &scm.maxZoom, 1.0f, 10.0f, 2000.0f);

                    const char* modes[] = { "Dolly Zoom (FOV)", "Realistic (Pullback)", "Visual Scale (KSP)" };
                    ImGui::Combo("Camera Mode", &scm.mode, modes, IM_ARRAYSIZE(modes));
                }
            } else if (m_focusedComponent == "ParticleSystem" && selected.has<ParticleSystemComponent>()) {
                if (drawComponentHeader("ParticleSystem", ICON_FA_FIRE, colRender, true, [&](){ selected.remove<ParticleSystemComponent>(); m_focusedComponent = ""; })) {
                    auto& ps = selected.get<ParticleSystemComponent>();
                    ImGui::Text("Max Particles: %d", static_cast<int>(ps.particlePool.size()));

                    if (ps.material) {
                        auto partMat = std::dynamic_pointer_cast<ParticleMaterial>(ps.material);
                        if (partMat) {
                            if (ImGui::TreeNodeEx(ICON_FA_PALETTE " Particle Material", ImGuiTreeNodeFlags_DefaultOpen)) {
                                auto TextureSlot = [&](const char* label, std::function<void(Ref<Texture>)> setter) {
                                    ImGui::PushID(label);
                                    if (ImGui::Button(ICON_FA_FOLDER_OPEN) && m_context->getEngine()) {
                                        auto selection = pfd::open_file("Load Particle Texture", ".", { "Image Files", "*.png *.jpg *.jpeg *.tga *.bmp" }).result();
                                        if (!selection.empty()) {
                                            try {
                                                auto tex = m_context->getEngine()->assets().load<Texture>(selection[0], true);
                                                setter(tex);
                                            } catch (const std::exception& e) { BB_CORE_ERROR("Failed to load texture: {}", e.what()); }
                                        }
                                    }
                                    ImGui::SameLine(); ImGui::Text("%s", label);
                                    ImGui::PopID();
                                };

                                TextureSlot("Base Texture", [&](Ref<Texture> t){ partMat->setBaseMap(t); });

                                // Fix N6: Uses m_particleTint member instead of static partCol
                                if (ImGui::ColorEdit4("Base Tint", &m_particleTint.x)) {
                                    partMat->setColor({m_particleTint.r, m_particleTint.g, m_particleTint.b}, m_particleTint.a);
                                }

                                ImGui::TreePop();
                            }
                        }
                    }

                    ImGui::Separator();
                    ImGui::Checkbox("Inject into Physics", &ps.injectIntoPhysics);
                }
            } else if (m_focusedComponent == "Selectable" && selected.has<SelectableComponent>()) {
                if (drawComponentHeader("Selectable", ICON_FA_ARROW_POINTER, colLogic, true, [&](){ selected.remove<SelectableComponent>(); m_focusedComponent = ""; })) {
                    auto& sel = selected.get<SelectableComponent>();
                    ImGui::Checkbox("Is Selectable", &sel.selectable);
                    ImGui::TextWrapped("Entities without this component are selectable by default (Opt-out).");
                }
            }
        }

        ImGui::Separator();
        if (ImGui::Button(ICON_FA_PLUS " Add Component")) ImGui::OpenPopup("AddComponentPopup");
        if (ImGui::BeginPopup("AddComponentPopup")) {
            if (ImGui::MenuItem("Camera")) selected.add<CameraComponent>();
            if (ImGui::MenuItem("FPS Controller")) selected.add<FPSControllerComponent>();
            if (ImGui::MenuItem("Orbit Controller")) selected.add<OrbitControllerComponent>();
            if (ImGui::MenuItem("Light")) selected.add<LightComponent>();
            if (ImGui::MenuItem("Skybox")) selected.add<SkyboxComponent>();
            if (ImGui::MenuItem("SkySphere")) selected.add<SkySphereComponent>();
            if (ImGui::MenuItem("Physics Body")) {
                selected.add<PhysicsComponent>();
                if (m_context->getEngine()) {
                    m_context->getEngine()->GetPhysicsWorld()->createRigidBody(selected);
                }
            }
            if (ImGui::MenuItem("Audio Source")) selected.add<AudioSourceComponent>();
            if (ImGui::MenuItem("Audio Listener")) selected.add<AudioListenerComponent>();
            if (ImGui::MenuItem("Simple Animation")) selected.add<SimpleAnimationComponent>();
            if (ImGui::MenuItem("Point Gravity Source")) selected.add<PointGravitySourceComponent>();
            if (ImGui::MenuItem("Spaceship Controller")) selected.add<SpaceshipControllerComponent>();
            if (ImGui::MenuItem("Smart Camera")) selected.add<SmartCameraComponent>();
            if (ImGui::MenuItem("Particle System")) selected.add<ParticleSystemComponent>();
            if (ImGui::MenuItem("Selectable (Opt-out)")) selected.add<SelectableComponent>();
            ImGui::EndPopup();
        }
    } else {
        ImGui::TextDisabled("Select an entity to view its properties.");
    }
    ImGui::End();
}

void InspectorPanel::drawComponentList(Entity entity) {
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

void InspectorPanel::drawMaterialUI(Material* material) {
    if (!material) return;
    if (ImGui::TreeNodeEx(ICON_FA_PALETTE " Material Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto pbrMat = dynamic_cast<PBRMaterial*>(material);
        if (pbrMat) {
            glm::vec3 color = pbrMat->getColor();
            if (ImGui::ColorEdit3("Albedo Color", &color.x)) {
                pbrMat->setColor(color);
                Entity sel = m_context->getSelectedEntity();
                if (sel && sel.has<MeshComponent>()) sel.get<MeshComponent>().color = color;
            }
            auto TextureSlot = [&](const char* label, bool isColor, std::function<void(Ref<Texture>)> setter) {
                ImGui::PushID(label);
                if (ImGui::Button(ICON_FA_FOLDER_OPEN) && m_context->getEngine()) {
                    auto selection = pfd::open_file("Load Texture", ".", { "Image Files", "*.png *.jpg *.jpeg *.tga *.bmp" }).result();
                    if (!selection.empty()) {
                        try {
                            auto tex = m_context->getEngine()->assets().load<Texture>(selection[0], isColor);
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
}

bool InspectorPanel::drawComponentHeader(const char* name, const char* icon, const ImVec4& color,
                                         bool canRemove, std::function<void()> onRemove) {
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
}

} // namespace bb3d::editor

#endif // BB3D_ENABLE_EDITOR
