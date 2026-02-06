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
        config.window.title = ICON_FA_GEAR " biobazard3d - Editor Stable Demo " ICON_FA_ROCKET;
        config.graphics.enableOffscreenRendering = true;
        
        auto engine = Engine::Create(config);
        auto scene = engine->CreateScene();
        
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

        while (!engine->window().ShouldClose()) {
            uint64_t currentTime = SDL_GetTicks();
            float deltaTime = static_cast<float>(currentTime - lastTime) / 1000.0f;
            lastTime = currentTime;
            if (deltaTime > 0.1f) deltaTime = 0.1f;

            engine->window().PollEvents();

#if defined(BB3D_ENABLE_EDITOR)
            auto& editor = engine->editor();
            editor.beginFrame();

            ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

            // Viewport Window
            ImGui::Begin(ICON_FA_GAMEPAD " Viewport");
            if (engine->renderer().getRenderTarget()) {
                auto rt = engine->renderer().getRenderTarget();
                static vk::ImageView lastView = nullptr;
                if (rt->getColorImageView() != lastView) {
                    viewportTexID = editor.addTexture(rt->getSampler(), rt->getColorImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
                    lastView = rt->getColorImageView();
                }
                ImGui::Image(viewportTexID, ImGui::GetContentRegionAvail());
            }
            ImGui::End();

            // Inspector
            ImGui::Begin(ICON_FA_CIRCLE_INFO " Inspector");
            ImGui::Text(ICON_FA_CUBE " Entity: Rotating Cube");
            ImGui::Separator();
            
            ImGui::Text("Appearance Test:");
            ImGui::BulletText("Base Font: Roboto (Smooth)");
            ImGui::BulletText("Icons: " ICON_FA_GHOST " " ICON_FA_FLOPPY_DISK " " ICON_FA_TRASH);
            ImGui::BulletText("Emojis: 🚀 🔥 💎 🛠️ 🍄 📦");
            
            ImGui::Separator();
            ImGui::SliderFloat("Rotation Speed", &rotationSpeed, 0.0f, 200.0f);
            
            auto& meshComp = cube.get<MeshComponent>();
            if (meshComp.mesh->getMaterial()) {
                auto mat = std::dynamic_pointer_cast<PBRMaterial>(meshComp.mesh->getMaterial());
                if (mat) {
                    glm::vec3 color = mat->getColor();
                    if (ImGui::ColorEdit3("Cube Color", &color.x)) mat->setColor(color);
                }
            }
            ImGui::End();

            // Stats
            ImGui::Begin(ICON_FA_CHART_LINE " Statistics");
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
            ImGui::End();
#endif

            cube.get<TransformComponent>().rotation.y += rotationSpeed * deltaTime;
            if (engine->GetActiveScene()) {
                if (engine->GetPhysicsWorld()) engine->GetPhysicsWorld()->update(deltaTime, *engine->GetActiveScene());
                engine->GetActiveScene()->onUpdate(deltaTime);
            }
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
