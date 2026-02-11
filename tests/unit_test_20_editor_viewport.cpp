#include "bb3d/core/Engine.hpp"
#include "bb3d/core/ImGuiLayer.hpp"
#include "bb3d/render/Renderer.hpp"
#include "bb3d/render/RenderTarget.hpp"
#include "bb3d/core/Log.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/scene/FPSCamera.hpp"
#include "bb3d/physics/PhysicsWorld.hpp"
#include "bb3d/render/MeshGenerator.hpp"
#include "bb3d/core/IconsFontAwesome6.h"
#include <imgui.h>
#include <SDL3/SDL.h>
#include <iostream>

using namespace bb3d;

int main() {
    try {
        EngineConfig config;
        config.window.title = ICON_FA_GEAR " biobazard3d - Editor Viewport " ICON_FA_ROCKET;
        config.graphics.enableOffscreenRendering = true;
        
        auto engine = Engine::Create(config);
        auto scene = engine->CreateScene();
        
        // Caméra initiale (FPS par défaut)
        scene->createFPSCamera("Main Camera", 45.0f, 1.77f, glm::vec3(0, 2, 8), engine.get());
        scene->createDirectionalLight("Sun", glm::vec3(1, 1, 1), 1.0f, glm::vec3(-45, 45, 0));
        
        // Sol physique
        auto floor = scene->createEntity("Floor");
        floor.add<MeshComponent>(MeshGenerator::createCheckerboardPlane(engine->graphics(), 20.0f, 20), "", PrimitiveType::Plane);
        floor.add<RigidBodyComponent>();
        floor.get<RigidBodyComponent>().type = BodyType::Static;
        floor.add<BoxColliderComponent>();
        floor.get<BoxColliderComponent>().halfExtents = {10.0f, 0.1f, 10.0f};
        
        engine->SetActiveScene(scene);

        ImTextureID viewportTexID = 0;
        uint64_t lastTime = SDL_GetTicks();
        static bool isOrbit = false;

        while (!engine->window().ShouldClose()) {
            // IMPORTANT: Reset des deltas (scroll, souris) au début de chaque frame
            engine->input().clearDeltas();

            uint64_t currentTime = SDL_GetTicks();
            float deltaTime = static_cast<float>(currentTime - lastTime) / 1000.0f;
            lastTime = currentTime;
            if (deltaTime > 0.1f) deltaTime = 0.1f;

            engine->window().PollEvents();

#if defined(BB3D_ENABLE_EDITOR)
            auto& editor = engine->editor();
            editor.beginFrame();

            editor.showMainMenu();
            ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

            editor.showSceneHierarchy(*engine->GetActiveScene());
            editor.showSceneSettings(*engine->GetActiveScene());
            editor.showInspector();
            editor.showToolbar();

            // --- VIEWPORT WINDOW ---
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); 
            ImGui::Begin(ICON_FA_GAMEPAD " Viewport");
            
            bool isHovered = ImGui::IsWindowHovered();
            bool isFocused = ImGui::IsWindowFocused();
            // On détecte si l'un des boutons de souris est maintenu dans le viewport
            bool isMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::IsMouseDown(ImGuiMouseButton_Right);

            if (isHovered && isMouseDown) {
                ImGui::SetWindowFocus();
            }
            editor.setViewportState(isFocused, isHovered);

            if (engine->renderer().getRenderTarget()) {
                auto rt = engine->renderer().getRenderTarget();
                static vk::ImageView lastView = nullptr;
                if (rt->getColorImageView() != lastView) {
                    viewportTexID = editor.addTexture(rt->getSampler(), rt->getColorImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
                    lastView = rt->getColorImageView();
                }
                
                ImVec2 availSize = ImGui::GetContentRegionAvail();
                if (availSize.x > 0 && availSize.y > 0) {
                    float targetAspect = (float)rt->getExtent().width / (float)rt->getExtent().height;
                    float currentAspect = availSize.x / availSize.y;
                    
                    ImVec2 imageSize = availSize;
                    if (currentAspect > targetAspect) {
                        imageSize.x = availSize.y * targetAspect;
                    } else {
                        imageSize.y = availSize.x / targetAspect;
                    }
                    
                    ImVec2 pos = ImGui::GetCursorPos();
                    pos.x += (availSize.x - imageSize.x) * 0.5f;
                    pos.y += (availSize.y - imageSize.y) * 0.5f;
                    ImGui::SetCursorPos(pos);
                    ImGui::Image(viewportTexID, imageSize);
                }
            }
            ImGui::End();
            ImGui::PopStyleVar();

            ImGui::Begin(ICON_FA_CHART_LINE " Statistics");
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
            ImGui::Text("Camera: %s", isOrbit ? "Orbit" : "FPS");
            ImGui::End();
#endif

            // --- INPUT HANDLING ---
            bool captureMouse = editor.wantCaptureMouse();
            bool captureKeyboard = editor.wantCaptureKeyboard();

            // Priorité absolue aux raccourcis du Viewport s'il est actif
            if (editor.isViewportFocused()) {
                captureKeyboard = false; 
                // Pour l'orbiteur (clic gauche) ou le FPS (clic droit), on libère la souris
                if (isMouseDown) captureMouse = false;
            }
            
            engine->input().update(captureMouse, captureKeyboard);

            if (engine->input().isKeyJustPressed(Key::C)) {
                isOrbit = !isOrbit;
                auto activeScene = engine->GetActiveScene();
                if (activeScene) {
                    auto view = activeScene->getRegistry().view<CameraComponent>();
                    for (auto entity : view) {
                        Entity e(entity, *activeScene);
                        if (isOrbit) {
                            e.remove<FPSControllerComponent>();
                            e.add<OrbitControllerComponent>();
                            auto& orbit = e.get<OrbitControllerComponent>();
                            orbit.target = glm::vec3(0, 0, 0);
                            orbit.distance = 15.0f;
                            orbit.pitch = 45.0f; // Angle plus prononcé
                            orbit.yaw = 45.0f;
                            BB_CORE_INFO("Switched to Orbit Camera");
                        } else {
                            e.remove<OrbitControllerComponent>();
                            e.add<FPSControllerComponent>();
                            e.get<TransformComponent>().translation = glm::vec3(0, 2, 8);
                            BB_CORE_INFO("Switched to FPS Camera");
                        }
                    }
                }
            }

            // --- UPDATE LOGIC ---
            if (engine->GetActiveScene()) {
                if (engine->GetPhysicsWorld() && !engine->isPhysicsPaused()) {
                    engine->GetPhysicsWorld()->update(deltaTime, *engine->GetActiveScene());
                }
                
                // On appelle onUpdate TOUJOURS pour que le Reset camera soit appliqué instantanément.
                // Le blocage des contrôles (mouvement clavier/souris) est géré par engine->input().update() plus haut.
                engine->GetActiveScene()->onUpdate(deltaTime);
            }

            // --- RENDER ---
            engine->renderer().render(*engine->GetActiveScene());

#if defined(BB3D_ENABLE_EDITOR)
            engine->renderer().renderUI([&](vk::CommandBuffer cb) { editor.endFrame(cb); });
#endif
            engine->renderer().submitAndPresent();
        }
    } catch (const std::exception& e) {
        std::cerr << "CRITICAL CRASH: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}