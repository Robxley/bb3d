#include "bb3d/core/Engine.hpp"
#include "bb3d/core/Log.hpp"
#include "bb3d/scene/FPSCamera.hpp"
#include "bb3d/scene/OrbitCamera.hpp"
#include "bb3d/render/MeshGenerator.hpp"
#include "bb3d/render/GraphicsPipeline.hpp"
#include "bb3d/render/UniformBuffer.hpp"
#include <SDL3/SDL.h>
#include <chrono>

struct GlobalUBO {
    glm::mat4 view;
    glm::mat4 proj;
};

void transitionLayout(vk::CommandBuffer cb, vk::Image img, vk::Format fmt, vk::ImageLayout oldL, vk::ImageLayout newL) {
    vk::ImageMemoryBarrier b({}, {}, oldL, newL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, img, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
    vk::PipelineStageFlags src, dst;
    if (oldL == vk::ImageLayout::eUndefined) {
        b.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        src = vk::PipelineStageFlagBits::eTopOfPipe; dst = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    } else {
        b.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        src = vk::PipelineStageFlagBits::eColorAttachmentOutput; dst = vk::PipelineStageFlagBits::eBottomOfPipe;
    }
    cb.pipelineBarrier(src, dst, {}, nullptr, nullptr, b);
}

void transitionDepth(vk::CommandBuffer cb, vk::Image img, vk::ImageLayout oldL, vk::ImageLayout newL) {
    vk::ImageMemoryBarrier b({}, vk::AccessFlagBits::eDepthStencilAttachmentWrite, oldL, newL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, img, { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 });
    if (oldL == vk::ImageLayout::eUndefined) {
        cb.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eEarlyFragmentTests, {}, nullptr, nullptr, b);
    }
}

int main() {
    bb3d::EngineConfig logConfig;
    logConfig.system.logDirectory = "unit_test_logs";
    bb3d::Log::Init(logConfig);
    BB_CORE_INFO("Test Unitaire 14 : Caméras Interactives");
    BB_CORE_INFO("Contrôles :");
    BB_CORE_INFO("  [C] : Changer de mode caméra (FPS / Orbit)");
    BB_CORE_INFO("  [Z/Q/S/D] : Déplacement (FPS)");
    BB_CORE_INFO("  [Souris + Clic Gauche] : Rotation");
    BB_CORE_INFO("  [Molette] : Zoom (Orbit)");

    try {
        bb3d::EngineConfig config;
        config.window.title = "BB3D - Camera Test";
        config.window.width = 1280;
        config.window.height = 720;
        
        bb3d::Window window(config);
        bb3d::VulkanContext context;
        context.init(window.GetNativeWindow(), "CameraTest", true);
        vk::Device dev = context.getDevice();

        { // Nested scope to ensure all RAII resources are destroyed before context.cleanup()
            bb3d::SwapChain swapChain(context, config.window.width, config.window.height);

            auto floor = bb3d::MeshGenerator::createCheckerboardPlane(context, 20.0f, 20);
            auto cube = bb3d::MeshGenerator::createCube(context, 1.0f, {1, 0, 0});
            auto sphere = bb3d::MeshGenerator::createSphere(context, 0.5f, 32, {0, 0, 1});
            auto centerSphere = bb3d::MeshGenerator::createSphere(context, 0.1f, 16, {0, 1, 0}); // Petite sphère verte au centre

            bb3d::Shader vert(context, "assets/shaders/simple_3d.vert.spv");
            bb3d::Shader frag(context, "assets/shaders/simple_3d.frag.spv");

            bb3d::UniformBuffer ubo(context, sizeof(GlobalUBO));
            vk::DescriptorSetLayoutBinding binding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex);
            vk::DescriptorSetLayout dsl = dev.createDescriptorSetLayout({ {}, 1, &binding });
            vk::DescriptorPoolSize poolSize(vk::DescriptorType::eUniformBuffer, 1);
            vk::DescriptorPool pool = dev.createDescriptorPool({ {}, 1, 1, &poolSize });
            vk::DescriptorSet ds = dev.allocateDescriptorSets({ pool, 1, &dsl })[0];
            vk::DescriptorBufferInfo bInfo(ubo.getHandle(), 0, sizeof(GlobalUBO));
            vk::WriteDescriptorSet write(ds, 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &bInfo);
            dev.updateDescriptorSets(1, &write, 0, nullptr);

            vk::PushConstantRange pcr(vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4));
            bb3d::GraphicsPipeline pipeline(context, swapChain, vert, frag, config, {dsl}, {pcr});

            auto fpsCam = bb3d::CreateScope<bb3d::FPSCamera>(45.0f, 1280.0f/720.0f, 0.1f, 100.0f);
            auto orbitCam = bb3d::CreateScope<bb3d::OrbitCamera>(45.0f, 1280.0f/720.0f, 0.1f, 100.0f);
            fpsCam->setPosition({0, 2, 5});
            orbitCam->setTarget({0, 0, 0});

            bool isFPS = true;
            bb3d::Camera* activeCam = fpsCam.get();

            vk::CommandPool cp = dev.createCommandPool({ vk::CommandPoolCreateFlagBits::eResetCommandBuffer, context.getGraphicsQueueFamily() });
            
            // On utilise autant de frames en vol que d'images dans la swapchain pour éviter les collisions de sémaphores
            const uint32_t MAX_FRAMES = static_cast<uint32_t>(swapChain.getImageCount());
            
            auto cbs = dev.allocateCommandBuffers({ cp, vk::CommandBufferLevel::ePrimary, MAX_FRAMES });
            std::vector<vk::Semaphore> semA(MAX_FRAMES), semR(MAX_FRAMES);
            std::vector<vk::Fence> fen(MAX_FRAMES);
            for(uint32_t i=0; i<MAX_FRAMES; ++i) { 
                semA[i]=dev.createSemaphore({}); semR[i]=dev.createSemaphore({}); 
                fen[i]=dev.createFence({vk::FenceCreateFlagBits::eSignaled});
            }

            auto lastTime = std::chrono::high_resolution_clock::now();
            uint32_t frameIdx = 0;
            bool leftMouseDown = false;

            while (!window.ShouldClose()) {
                auto now = std::chrono::high_resolution_clock::now();
                float dt = std::chrono::duration<float>(now - lastTime).count();
                lastTime = now;

                SDL_Event e;
                while (SDL_PollEvent(&e)) {
                    if (e.type == SDL_EVENT_QUIT) window.PollEvents(); 
                    if (e.type == SDL_EVENT_KEY_DOWN) {
                        if (e.key.key == SDLK_ESCAPE) goto end_loop;
                        if (e.key.key == SDLK_C) {
                            isFPS = !isFPS;
                            activeCam = isFPS ? (bb3d::Camera*)fpsCam.get() : (bb3d::Camera*)orbitCam.get();
                            BB_CORE_INFO("Mode Caméra: {}", isFPS ? "FPS" : "Orbit");
                        }
                    }
                    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) leftMouseDown = true;
                    if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT) leftMouseDown = false;
                    if (e.type == SDL_EVENT_MOUSE_MOTION && leftMouseDown) {
                        if (isFPS) fpsCam->rotate(e.motion.xrel, -e.motion.yrel);
                        else orbitCam->rotate(e.motion.xrel, -e.motion.yrel);
                    }
                    if (e.type == SDL_EVENT_MOUSE_WHEEL && !isFPS) orbitCam->zoom(e.wheel.y);
                }

                if (isFPS) {
                    const bool* state = SDL_GetKeyboardState(NULL);
                    glm::vec3 dir(0);
                    if (state[SDL_SCANCODE_W]) dir.z += 1;
                    if (state[SDL_SCANCODE_S]) dir.z -= 1;
                    if (state[SDL_SCANCODE_A]) dir.x -= 1;
                    if (state[SDL_SCANCODE_D]) dir.x += 1;
                    fpsCam->move(dir, dt);
                }
                activeCam->update(dt);

                (void)dev.waitForFences(1, &fen[frameIdx], VK_TRUE, UINT64_MAX); 
                (void)dev.resetFences(1, &fen[frameIdx]);
                uint32_t imgIdx;
                try { imgIdx = swapChain.acquireNextImage(semA[frameIdx]); } catch (...) { continue; }

                auto& cb = cbs[frameIdx];
                cb.reset({}); cb.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
                transitionLayout(cb, swapChain.getImage(imgIdx), swapChain.getImageFormat(), vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
                transitionDepth(cb, swapChain.getDepthImage(), vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);

                vk::RenderingAttachmentInfo cAttr(swapChain.getImageViews()[imgIdx], vk::ImageLayout::eColorAttachmentOptimal, vk::ResolveModeFlagBits::eNone, {}, vk::ImageLayout::eUndefined, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::ClearValue(vk::ClearColorValue(std::array<float,4>{0.1f,0.1f,0.1f,1.0f})));
                vk::RenderingAttachmentInfo dAttr(swapChain.getDepthImageView(), vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::ResolveModeFlagBits::eNone, {}, vk::ImageLayout::eUndefined, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::ClearValue(vk::ClearDepthStencilValue(1.0f, 0)));
                vk::RenderingInfo rI({}, {{0,0}, swapChain.getExtent()}, 1, 0, 1, &cAttr, &dAttr);

                cb.beginRendering(rI);
                pipeline.bind(cb);
                cb.setViewport(0, vk::Viewport(0,0,(float)swapChain.getExtent().width, (float)swapChain.getExtent().height, 0, 1));
                cb.setScissor(0, vk::Rect2D({0,0}, swapChain.getExtent()));
                cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline.getLayout(), 0, 1, &ds, 0, nullptr);

                GlobalUBO data; data.view = activeCam->getViewMatrix(); data.proj = activeCam->getProjectionMatrix();
                ubo.update(&data, sizeof(data));

                // 1. Floor
                glm::mat4 model = glm::mat4(1.0f);
                cb.pushConstants(pipeline.getLayout(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4), &model);
                floor->draw(cb);

                // 2. Cube
                model = glm::translate(glm::mat4(1.0f), {2, 0.5f, 0});
                cb.pushConstants(pipeline.getLayout(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4), &model);
                cube->draw(cb);

                // 3. Sphère
                model = glm::translate(glm::mat4(1.0f), {-2, 0.5f, 0});
                cb.pushConstants(pipeline.getLayout(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4), &model);
                sphere->draw(cb);

                // 4. Point d'orbite (Verte)
                model = glm::translate(glm::mat4(1.0f), {0, 0, 0});
                cb.pushConstants(pipeline.getLayout(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4), &model);
                centerSphere->draw(cb);

                cb.endRendering();
                transitionLayout(cb, swapChain.getImage(imgIdx), swapChain.getImageFormat(), vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR);
                cb.end();

                vk::PipelineStageFlags wait = vk::PipelineStageFlagBits::eColorAttachmentOutput;
                dev.getQueue(context.getGraphicsQueueFamily(), 0).submit(vk::SubmitInfo(1, &semA[frameIdx], &wait, 1, &cb, 1, &semR[frameIdx]), fen[frameIdx]);
                swapChain.present(semR[frameIdx], imgIdx);
                frameIdx = (frameIdx + 1) % MAX_FRAMES;
            }
            end_loop:
            dev.waitIdle();
            for(int i=0; i<MAX_FRAMES; ++i) { dev.destroySemaphore(semA[i]); dev.destroySemaphore(semR[i]); dev.destroyFence(fen[i]); }
            dev.destroyCommandPool(cp); dev.destroyDescriptorPool(pool); dev.destroyDescriptorSetLayout(dsl);
        } // Nested objects destroyed here, BEFORE context

    } catch (const std::exception& e) { BB_CORE_ERROR("Fail: {}", e.what()); return -1; }
    return 0;
}