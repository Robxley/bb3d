#include "bb3d/core/Log.hpp"
#include "bb3d/core/Config.hpp"
#include "bb3d/core/Window.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/render/SwapChain.hpp"
#include "bb3d/render/Shader.hpp"
#include "bb3d/render/GraphicsPipeline.hpp"
#include "bb3d/render/Buffer.hpp"
#include "bb3d/render/Vertex.hpp"
#include <iostream>
#include <vector>
#include <array>

void transitionImageLayout(vk::CommandBuffer commandBuffer, vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
    vk::ImageMemoryBarrier barrier({}, {}, oldLayout, newLayout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

    vk::PipelineStageFlags sourceStage;
    vk::PipelineStageFlags destinationStage;

    if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eColorAttachmentOptimal) {
        barrier.srcAccessMask = vk::AccessFlags();
        barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    } else if (oldLayout == vk::ImageLayout::eColorAttachmentOptimal && newLayout == vk::ImageLayout::ePresentSrcKHR) {
        barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        barrier.dstAccessMask = vk::AccessFlags();
        sourceStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        destinationStage = vk::PipelineStageFlagBits::eBottomOfPipe;
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, nullptr, nullptr, barrier);
}

int main() {
    bb3d::EngineConfig logConfig;
    logConfig.system.logDirectory = "unit_test_logs";
    bb3d::Log::Init(logConfig);
    BB_CORE_INFO("Test Unitaire 05 : Vertex Buffer & VMA (Vulkan-Hpp)");

    try {
        bb3d::EngineConfig config;
        config.window.title = "BB3D - Vertex Buffer Quad";
        config.window.width = 800;
        config.window.height = 600;
        config.rasterizer.cullMode = "None";
        config.depthStencil.depthTest = false;
        config.depthStencil.depthWrite = false;
        bb3d::Window window(config);

        bb3d::VulkanContext context;
        context.init(window.GetNativeWindow(), "Vertex Buffer Test", true);

        bb3d::SwapChain swapChain(context, config.window.width, config.window.height);

        // --- Shaders ---
        bb3d::Shader vertShader(context, "assets/shaders/triangle_buffer.vert.spv");
        bb3d::Shader fragShader(context, "assets/shaders/triangle.frag.spv");

        const std::vector<bb3d::Vertex> vertices = {
            {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
            {{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
            {{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
            {{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
        };
        const std::vector<uint32_t> indices = { 0, 1, 2, 2, 3, 0 };

        vk::DeviceSize vSize = vertices.size() * sizeof(bb3d::Vertex);
        bb3d::Buffer vertexBuffer(context, vSize, vk::BufferUsageFlagBits::eVertexBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT);
        vertexBuffer.upload(vertices.data(), vSize);

        vk::DeviceSize iSize = indices.size() * sizeof(uint32_t);
        bb3d::Buffer indexBuffer(context, iSize, vk::BufferUsageFlagBits::eIndexBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT);
        indexBuffer.upload(indices.data(), iSize);

        bb3d::GraphicsPipeline pipeline(context, swapChain, vertShader, fragShader, config);

        vk::Device device = context.getDevice();
        vk::CommandPool commandPool = device.createCommandPool({ vk::CommandPoolCreateFlagBits::eResetCommandBuffer, context.getGraphicsQueueFamily() });
        vk::CommandBuffer commandBuffer = device.allocateCommandBuffers({ commandPool, vk::CommandBufferLevel::ePrimary, 1 })[0];

        vk::Semaphore imageAvailable = device.createSemaphore({});
        vk::Semaphore renderFinished = device.createSemaphore({});

        BB_CORE_INFO("Démarrage du rendu du Quad...");

        while (!window.ShouldClose()) {
            window.PollEvents();

            uint32_t imageIndex = swapChain.acquireNextImage(imageAvailable);

            commandBuffer.reset({});
            commandBuffer.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

            vk::Image image = swapChain.getImage(imageIndex);
            transitionImageLayout(commandBuffer, image, swapChain.getImageFormat(), vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);

            vk::RenderingAttachmentInfo colorAttachment(swapChain.getImageViews()[imageIndex], vk::ImageLayout::eColorAttachmentOptimal, vk::ResolveModeFlagBits::eNone, {}, vk::ImageLayout::eUndefined, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::ClearValue(vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f})));

            vk::RenderingInfo renderingInfo({}, {{0, 0}, swapChain.getExtent()}, 1, 0, 1, &colorAttachment);

            commandBuffer.beginRendering(renderingInfo);
            pipeline.bind(commandBuffer);

            commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, (float)swapChain.getExtent().width, (float)swapChain.getExtent().height, 0.0f, 1.0f));
            commandBuffer.setScissor(0, vk::Rect2D({0, 0}, swapChain.getExtent()));

            vk::Buffer vbs[] = { vertexBuffer.getHandle() };
            vk::DeviceSize offsets[] = { 0 };
            commandBuffer.bindVertexBuffers(0, 1, vbs, offsets);
            commandBuffer.bindIndexBuffer(indexBuffer.getHandle(), 0, vk::IndexType::eUint32);

            commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
            commandBuffer.endRendering();

            transitionImageLayout(commandBuffer, image, swapChain.getImageFormat(), vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR);
            commandBuffer.end();

            vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
            vk::SubmitInfo submitInfo(1, &imageAvailable, waitStages, 1, &commandBuffer, 1, &renderFinished);

            context.getGraphicsQueue().submit(submitInfo, nullptr);
            
            swapChain.present(renderFinished, imageIndex);
            context.getGraphicsQueue().waitIdle();
        }

        device.waitIdle();
        device.destroySemaphore(imageAvailable);
        device.destroySemaphore(renderFinished);
        device.destroyCommandPool(commandPool);

    } catch (const std::exception& e) {
        BB_CORE_ERROR("Erreur : {}", e.what());
        return -1;
    }

    return 0;
}
