#include "bb3d/core/Log.hpp"
#include "bb3d/core/Config.hpp"
#include "bb3d/core/Window.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/render/SwapChain.hpp"
#include "bb3d/render/Shader.hpp"
#include "bb3d/render/GraphicsPipeline.hpp"
#include "bb3d/render/Buffer.hpp"
#include "bb3d/render/UniformBuffer.hpp"
#include "bb3d/render/Texture.hpp"
#include "bb3d/render/Vertex.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <vector>
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
    bb3d::Log::Init();
    try {
        bb3d::EngineConfig config; config.window.title = "BB3D - Texture Cube (Vulkan-Hpp)";
        bb3d::Window window(config);
        bb3d::VulkanContext context; context.init(window.GetNativeWindow(), "Texture Cube", true);
        bb3d::SwapChain swapChain(context, config.window.width, config.window.height);

        bb3d::Shader vert(context, "assets/shaders/texture_cube.vert.spv");
        bb3d::Shader frag(context, "assets/shaders/texture_cube.frag.spv");

        std::vector<bb3d::Vertex> vertices = {
            {{-0.5,-0.5, 0.5},{0,0,1},{1,1,1},{0,0}}, {{ 0.5,-0.5, 0.5},{0,0,1},{1,1,1},{1,0}}, {{ 0.5, 0.5, 0.5},{0,0,1},{1,1,1},{1,1}},
            {{-0.5,-0.5, 0.5},{0,0,1},{1,1,1},{0,0}}, {{ 0.5, 0.5, 0.5},{0,0,1},{1,1,1},{1,1}}, {{-0.5, 0.5, 0.5},{0,0,1},{1,1,1},{0,1}},
            {{ 0.5,-0.5,-0.5},{0,0,-1},{1,1,1},{0,0}}, {{-0.5,-0.5,-0.5},{0,0,-1},{1,1,1},{1,0}}, {{-0.5, 0.5,-0.5},{0,0,-1},{1,1,1},{1,1}},
            {{ 0.5,-0.5,-0.5},{0,0,-1},{1,1,1},{0,0}}, {{-0.5, 0.5,-0.5},{0,0,-1},{1,1,1},{1,1}}, {{ 0.5, 0.5,-0.5},{0,0,-1},{1,1,1},{0,1}},
            {{-0.5,-0.5,-0.5},{-1,0,0},{1,1,1},{0,0}}, {{-0.5,-0.5, 0.5},{-1,0,0},{1,1,1},{1,0}}, {{-0.5, 0.5, 0.5},{-1,0,0},{1,1,1},{1,1}},
            {{-0.5,-0.5,-0.5},{-1,0,0},{1,1,1},{0,0}}, {{-0.5, 0.5, 0.5},{-1,0,0},{1,1,1},{1,1}}, {{-0.5, 0.5,-0.5},{-1,0,0},{1,1,1},{0,1}},
            {{ 0.5,-0.5, 0.5},{1,0,0},{1,1,1},{0,0}}, {{ 0.5,-0.5,-0.5},{1,0,0},{1,1,1},{1,0}}, {{ 0.5, 0.5,-0.5},{1,0,0},{1,1,1},{1,1}},
            {{ 0.5,-0.5, 0.5},{1,0,0},{1,1,1},{0,0}}, {{ 0.5, 0.5,-0.5},{1,0,0},{1,1,1},{1,1}}, {{ 0.5, 0.5, 0.5},{1,0,0},{1,1,1},{0,1}},
            {{-0.5, 0.5, 0.5},{0,1,0},{1,1,1},{0,0}}, {{ 0.5, 0.5, 0.5},{0,1,0},{1,1,1},{1,0}}, {{ 0.5, 0.5,-0.5},{0,1,0},{1,1,1},{1,1}},
            {{-0.5, 0.5, 0.5},{0,1,0},{1,1,1},{0,0}}, {{ 0.5, 0.5,-0.5},{0,1,0},{1,1,1},{1,1}}, {{-0.5, 0.5,-0.5},{0,1,0},{1,1,1},{0,1}},
            {{-0.5,-0.5,-0.5},{0,-1,0},{1,1,1},{0,0}}, {{ 0.5,-0.5,-0.5},{0,-1,0},{1,1,1},{1,0}}, {{ 0.5,-0.5, 0.5},{0,-1,0},{1,1,1},{1,1}},
            {{-0.5,-0.5,-0.5},{0,-1,0},{1,1,1},{0,0}}, {{ 0.5,-0.5, 0.5},{0,-1,0},{1,1,1},{1,1}}, {{-0.5,-0.5, 0.5},{0,-1,0},{1,1,1},{0,1}}
        };

        vk::DeviceSize vS = vertices.size()*sizeof(vertices[0]);
        bb3d::Buffer vB(context, vS, vk::BufferUsageFlagBits::eVertexBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT);
        vB.upload(vertices.data(), vS);
        bb3d::Texture tex(context, "assets/textures/Bricks092_1K-JPG_Color.jpg");
        bb3d::UniformBuffer ubo(context, sizeof(UniformBufferObject));

        vk::Device dev = context.getDevice();
        vk::DescriptorSetLayoutBinding b[] = {{0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex}, {1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}};
        vk::DescriptorSetLayout dsl = dev.createDescriptorSetLayout({ {}, 2, b });
        vk::DescriptorPoolSize ps[] = {{vk::DescriptorType::eUniformBuffer, 1}, {vk::DescriptorType::eCombinedImageSampler, 1}};
        vk::DescriptorPool dp = dev.createDescriptorPool({ {}, 1, 2, ps });
        vk::DescriptorSet ds = dev.allocateDescriptorSets({ dp, 1, &dsl })[0];
        vk::DescriptorBufferInfo bi(ubo.getHandle(), 0, sizeof(UniformBufferObject));
        vk::DescriptorImageInfo ii(tex.getSampler(), tex.getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
        vk::WriteDescriptorSet w[] = {{ds, 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &bi}, {ds, 1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &ii}};
        dev.updateDescriptorSets(2, w, 0, nullptr);

        bb3d::GraphicsPipeline pipe(context, swapChain, vert, frag, config, {dsl});
        vk::CommandPool cp = dev.createCommandPool({ vk::CommandPoolCreateFlagBits::eResetCommandBuffer, context.getGraphicsQueueFamily() });
        vk::CommandBuffer cb = dev.allocateCommandBuffers({ cp, vk::CommandBufferLevel::ePrimary, 1 })[0];
        vk::Semaphore semA = dev.createSemaphore({}), semR = dev.createSemaphore({});
        vk::Fence fen = dev.createFence({ vk::FenceCreateFlagBits::eSignaled });

        while (!window.ShouldClose()) {
            window.PollEvents();
            static auto start = std::chrono::high_resolution_clock::now();
            float t = std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - start).count();
            UniformBufferObject data;
            data.model = glm::rotate(glm::mat4(1.0f), t, glm::vec3(0.5, 1, 0));
            data.view = glm::lookAt(glm::vec3(2,2,2), glm::vec3(0,0,0), glm::vec3(0,1,0));
            data.proj = glm::perspective(glm::radians(45.0f), (float)config.window.width/config.window.height, 0.1f, 10.f); data.proj[1][1] *= -1;
            ubo.update(&data, sizeof(data));

            auto wr = dev.waitForFences(1, &fen, VK_TRUE, UINT64_MAX);
            dev.resetFences(1, &fen);
            uint32_t idx = swapChain.acquireNextImage(semA);
            cb.reset({}); cb.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
            transitionImageLayout(cb, swapChain.getImage(idx), swapChain.getImageFormat(), vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
            transitionDepthLayout(cb, swapChain.getDepthImage(), vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);
            vk::RenderingAttachmentInfo cai(swapChain.getImageViews()[idx], vk::ImageLayout::eColorAttachmentOptimal, vk::ResolveModeFlagBits::eNone, {}, vk::ImageLayout::eUndefined, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::ClearValue(vk::ClearColorValue(std::array<float, 4>{0.1f, 0.1f, 0.1f, 1.0f})));
            vk::RenderingAttachmentInfo dai(swapChain.getDepthImageView(), vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::ResolveModeFlagBits::eNone, {}, vk::ImageLayout::eUndefined, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::ClearValue(vk::ClearDepthStencilValue(1.0f, 0)));
            vk::RenderingInfo ri({}, {{0,0}, swapChain.getExtent()}, 1, 0, 1, &cai, &dai);
            cb.beginRendering(ri); pipe.bind(cb);
            cb.setViewport(0, vk::Viewport(0,0,(float)swapChain.getExtent().width, (float)swapChain.getExtent().height, 0, 1));
            cb.setScissor(0, vk::Rect2D({0,0}, swapChain.getExtent()));
            cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipe.getLayout(), 0, 1, &ds, 0, nullptr);
            vk::Buffer vbs[] = {vB.getHandle()}; vk::DeviceSize off[] = {0}; cb.bindVertexBuffers(0, 1, vbs, off);
            cb.draw(static_cast<uint32_t>(vertices.size()), 1, 0, 0); cb.endRendering();
            transitionImageLayout(cb, swapChain.getImage(idx), swapChain.getImageFormat(), vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR);
            cb.end();
            vk::PipelineStageFlags wait = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            vk::SubmitInfo sub(1, &semA, &wait, 1, &cb, 1, &semR);
            context.getGraphicsQueue().submit(sub, fen);
            swapChain.present(semR, idx);
        }
        dev.waitIdle();
        dev.destroyFence(fen); dev.destroySemaphore(semA); dev.destroySemaphore(semR);
        dev.destroyDescriptorPool(dp); dev.destroyDescriptorSetLayout(dsl); dev.destroyCommandPool(cp);
    } catch (const std::exception& e) { BB_CORE_ERROR("Fail: {}", e.what()); return -1; }
    return 0;
}
