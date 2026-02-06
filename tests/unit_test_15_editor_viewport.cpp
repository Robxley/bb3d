#include "bb3d/core/Engine.hpp"
#include "bb3d/core/ImGuiLayer.hpp"
#include "bb3d/render/Renderer.hpp"
#include "bb3d/render/RenderTarget.hpp"
#include "bb3d/core/Log.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/scene/FPSCamera.hpp"
#include "bb3d/physics/PhysicsWorld.hpp"
#include "bb3d/render/MeshGenerator.hpp"
#include <imgui.h>
#include <SDL3/SDL.h>

#include <iostream>

using namespace bb3d;

int main() {
    try {
        std::cout << "Starting application..." << std::endl;
        EngineConfig config;
        config.window.title = "biobazard3d - ImGui & Viewport Demo 🚀";
        config.graphics.enableOffscreenRendering = true;
        
        std::cout << "Creating engine..." << std::endl;
        auto engine = Engine::Create(config);
        std::cout << "Engine created successfully." << std::endl;

                auto scene = engine->CreateScene();
                std::cout << "Scene created." << std::endl;
                
                // On utilise les fonctions de haut niveau pour éviter les doublons de composants (qui font crasher EnTT)
                scene->createFPSCamera("Main Camera", 45.0f, 1.77f, glm::vec3(0, 2, 8), engine.get());
        
                scene->createDirectionalLight("Sun", glm::vec3(1, 1, 1), 1.0f, glm::vec3(-45, 45, 0));
        
                auto floor = scene->createEntity("Floor");
                floor.add<MeshComponent>(MeshGenerator::createCheckerboardPlane(engine->graphics(), 20.0f, 20));
        
                auto cube = scene->createEntity("Rotating Cube");
                cube.add<MeshComponent>(MeshGenerator::createCube(engine->graphics(), 2.0f, glm::vec3(1, 0, 0)));
                cube.at(glm::vec3(0, 1, 0));
        
                engine->SetActiveScene(scene);
            ImTextureID viewportTexID = 0;
    float rotationSpeed = 50.0f;
    uint64_t lastTime = SDL_GetTicks();

    BB_CORE_INFO("Starting Custom Loop for Editor Demo...");

    while (!engine->window().ShouldClose()) {
        uint64_t currentTime = SDL_GetTicks();
        float deltaTime = static_cast<float>(currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;
        if (deltaTime > 0.1f) deltaTime = 0.1f;

        engine->window().PollEvents();

#if defined(BB3D_ENABLE_EDITOR)
        auto& editor = engine->editor();
        editor.beginFrame();

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Exit", "Alt+F4")) engine->Stop();
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        ImGui::Begin("🎮 Viewport");
        if (engine->renderer().getRenderTarget()) {
            auto rt = engine->renderer().getRenderTarget();
            
            // On vérifie si la texture a changé (redimensionnement)
            static vk::ImageView lastView = nullptr;
            if (rt->getColorImageView() != lastView) {
                viewportTexID = editor.addTexture(rt->getSampler(), rt->getColorImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
                lastView = rt->getColorImageView();
            }

            ImVec2 viewportSize = ImGui::GetContentRegionAvail();
            ImGui::Image(viewportTexID, viewportSize);
        }
        ImGui::End();

        ImGui::Begin("🔍 Inspector");
        ImGui::Text("Entity: Rotating Cube");
        ImGui::SliderFloat("Rotation Speed", &rotationSpeed, 0.0f, 200.0f);
        ImGui::Separator();
        ImGui::Text("Emoji Test: 🚀 🔥 💎 🛠️");
        
        auto& meshComp = cube.get<MeshComponent>();
        if (meshComp.mesh->getMaterial()) {
            auto mat = std::dynamic_pointer_cast<PBRMaterial>(meshComp.mesh->getMaterial());
            if (mat) {
                glm::vec3 color = mat->getColor();
                if (ImGui::ColorEdit3("Cube Color", &color.x)) {
                    mat->setColor(color);
                }
            }
        }
        ImGui::End();

        ImGui::Begin("📈 Statistics");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("GPU: %s", engine->graphics().getDeviceName().data());
        ImGui::End();
#endif

        cube.get<TransformComponent>().rotation.y += rotationSpeed * deltaTime;

        // On appelle manuellement Update et Render pour avoir le contrôle
        // Note: Normalement on utiliserait Engine::Run() et on s'injecterait via des callbacks.
        // Ici on simule Engine::Run().
        
        // update(deltaTime) est privé, mais on peut utiliser le moteur normalement
        // Pour cette démo, on accède aux membres via les accesseurs
        if (engine->GetActiveScene()) {
            if (engine->GetPhysicsWorld()) engine->GetPhysicsWorld()->update(deltaTime, *engine->GetActiveScene());
            engine->GetActiveScene()->onUpdate(deltaTime);
        }

        engine->renderer().render(*engine->GetActiveScene());

#if defined(BB3D_ENABLE_EDITOR)
        engine->renderer().renderUI([&](vk::CommandBuffer cb) {
            editor.endFrame(cb);
        });
#endif
        engine->renderer().submitAndPresent();
    }
            } catch (const std::exception& e) {
                std::cerr << "CRITICAL CRASH: " << e.what() << std::endl;
                return 1;
            } catch (...) {
                std::cerr << "UNKNOWN CRITICAL CRASH" << std::endl;
                return 1;
            }
        
            return 0;
        }
        