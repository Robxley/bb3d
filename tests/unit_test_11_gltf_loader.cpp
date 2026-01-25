#include "bb3d/core/Log.hpp"
#include "bb3d/core/Config.hpp"
#include "bb3d/core/Window.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/render/SwapChain.hpp"
#include "bb3d/render/Shader.hpp"
#include "bb3d/render/GraphicsPipeline.hpp"
#include "bb3d/render/UniformBuffer.hpp"
#include "bb3d/render/Model.hpp"
#include "bb3d/render/Texture.hpp"
#include "bb3d/resource/ResourceManager.hpp"
#include "bb3d/core/JobSystem.hpp"
#include "bb3d/scene/FPSCamera.hpp"
#include "bb3d/scene/Camera.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>
#include <chrono>
#include <array>

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

void transitionImageLayout(vk::CommandBuffer cb, vk::Image image, vk::Format format, vk::ImageLayout oldL, vk::ImageLayout newL) {
    vk::ImageMemoryBarrier b({}, {}, oldL, newL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
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

void transitionDepthLayout(vk::CommandBuffer cb, vk::Image image, vk::ImageLayout oldL, vk::ImageLayout newL) {
    vk::ImageMemoryBarrier b({}, vk::AccessFlagBits::eDepthStencilAttachmentWrite, oldL, newL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image, { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 });
    if (oldL == vk::ImageLayout::eUndefined) {
        cb.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eEarlyFragmentTests, {}, nullptr, nullptr, b);
    }
}

int main() {
    bb3d::EngineConfig logConfig;
    logConfig.system.logDirectory = "unit_test_logs";
    bb3d::Log::Init(logConfig);
    try {
        bb3d::EngineConfig config;
        config.window.title = "BB3D - GLTF Loader (Vulkan-Hpp)";
        config.window.width = 1280;
        config.window.height = 720;
        bb3d::Window window(config);

        bb3d::VulkanContext context;
        context.init(window.GetNativeWindow(), "GLTF Test", true);
        vk::Device device = context.getDevice();

        bb3d::SwapChain swapChain(context, config.window.width, config.window.height);
        bb3d::JobSystem jobSystem; jobSystem.init();
        bb3d::ResourceManager resources(context, jobSystem);

        auto model = resources.load<bb3d::Model>("assets/models/ant.glb");
        auto texture = model->getTexture(0);
        if (!texture) throw std::runtime_error("Texture manquante !");

        auto bounds = model->getBounds();
        glm::vec3 center = bounds.center(), size = bounds.size();
        float maxDim = std::max({size.x, size.y, size.z});
        float fov = 45.0f, dist = maxDim / std::tan(glm::radians(fov) * 0.5f) * 1.5f;

        bb3d::UniformBuffer ubo(context, sizeof(UniformBufferObject));
        vk::DescriptorSetLayoutBinding b[] = {{0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex}, {1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}};
        vk::DescriptorSetLayout dsl = device.createDescriptorSetLayout({ {}, 2, b });
        vk::DescriptorPoolSize ps[] = {{vk::DescriptorType::eUniformBuffer, 1}, {vk::DescriptorType::eCombinedImageSampler, 1}};
        vk::DescriptorPool dp = device.createDescriptorPool({ {}, 1, 2, ps });
        vk::DescriptorSet ds = device.allocateDescriptorSets({ dp, 1, &dsl })[0];

        vk::DescriptorBufferInfo bi(ubo.getHandle(), 0, sizeof(UniformBufferObject));
        vk::DescriptorImageInfo ii(texture->getSampler(), texture->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
        vk::WriteDescriptorSet w[] = {{ds, 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &bi}, {ds, 1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &ii}};
        device.updateDescriptorSets(2, w, 0, nullptr);

        // --- Pipeline ---
        bb3d::Shader vert(context, "assets/shaders/textured_mesh.vert.spv");
        bb3d::Shader frag(context, "assets/shaders/textured_mesh.frag.spv");
        bb3d::GraphicsPipeline pipeline(context, swapChain, vert, frag, config, {dsl});

        vk::CommandPool cp = device.createCommandPool({ vk::CommandPoolCreateFlagBits::eResetCommandBuffer, context.getGraphicsQueueFamily() });

        const int MAX_FRAMES_IN_FLIGHT = 2;
        std::vector<vk::CommandBuffer> commandBuffers = device.allocateCommandBuffers({ cp, vk::CommandBufferLevel::ePrimary, (uint32_t)MAX_FRAMES_IN_FLIGHT });

        std::vector<vk::Semaphore> semA(MAX_FRAMES_IN_FLIGHT), semR(MAX_FRAMES_IN_FLIGHT);
        std::vector<vk::Fence> inFlightFences(MAX_FRAMES_IN_FLIGHT);
        for(int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) { 
            semA[i] = device.createSemaphore({}); 
            semR[i] = device.createSemaphore({}); 
            inFlightFences[i] = device.createFence({ vk::FenceCreateFlagBits::eSignaled });
        }

        bb3d::FPSCamera camera(fov, (float)config.window.width/config.window.height, 0.1f, dist*10.0f);
        camera.setPosition({0, maxDim*0.5f, dist}); camera.lookAt({0,0,0});

        uint32_t currentFrame = 0;
        while (!window.ShouldClose()) {
            window.PollEvents();
            static auto start = std::chrono::high_resolution_clock::now();
            float t = std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - start).count();
            UniformBufferObject data{ glm::rotate(glm::mat4(1.0f), t*0.5f, {0,1,0}) * glm::translate(glm::mat4(1.0f), -center), camera.getViewMatrix(), camera.getProjectionMatrix() };
            ubo.update(&data, sizeof(data));

            auto wr = device.waitForFences(1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX); 
            device.resetFences(1, &inFlightFences[currentFrame]);

            uint32_t idx = swapChain.acquireNextImage(semA[currentFrame]);
            auto& cb = commandBuffers[currentFrame];
            cb.reset({}); cb.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
            transitionImageLayout(cb, swapChain.getImage(idx), swapChain.getImageFormat(), vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
            transitionDepthLayout(cb, swapChain.getDepthImage(), vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);
            vk::RenderingAttachmentInfo cAttr(swapChain.getImageViews()[idx], vk::ImageLayout::eColorAttachmentOptimal, vk::ResolveModeFlagBits::eNone, {}, vk::ImageLayout::eUndefined, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::ClearValue(vk::ClearColorValue(std::array<float,4>{0.1f,0.1f,0.1f,1.0f})));
            vk::RenderingAttachmentInfo dAttr(swapChain.getDepthImageView(), vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::ResolveModeFlagBits::eNone, {}, vk::ImageLayout::eUndefined, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::ClearValue(vk::ClearDepthStencilValue(1.0f, 0)));
            vk::RenderingInfo rI({}, {{0,0}, swapChain.getExtent()}, 1, 0, 1, &cAttr, &dAttr);
            cb.beginRendering(rI); pipeline.bind(cb);
            cb.setViewport(0, vk::Viewport(0,0,(float)swapChain.getExtent().width, (float)swapChain.getExtent().height, 0, 1));
            cb.setScissor(0, vk::Rect2D({0,0}, swapChain.getExtent()));
            cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline.getLayout(), 0, 1, &ds, 0, nullptr);
            model->draw(cb); cb.endRendering();
            transitionImageLayout(cb, swapChain.getImage(idx), swapChain.getImageFormat(), vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR);
            cb.end();
            vk::PipelineStageFlags wait = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            device.getQueue(context.getGraphicsQueueFamily(), 0).submit(vk::SubmitInfo(1, &semA[currentFrame], &wait, 1, &cb, 1, &semR[currentFrame]), inFlightFences[currentFrame]);
            swapChain.present(semR[currentFrame], idx); 
            currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        }
        device.waitIdle();
        for(int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) { 
            device.destroySemaphore(semA[i]); 
            device.destroySemaphore(semR[i]); 
            device.destroyFence(inFlightFences[i]);
        }
        device.destroyCommandPool(cp); device.destroyDescriptorPool(dp); device.destroyDescriptorSetLayout(dsl);
        jobSystem.shutdown();
    } catch (const std::exception& e) { BB_CORE_ERROR("Fail: {}", e.what()); return -1; }
    return 0;
}
