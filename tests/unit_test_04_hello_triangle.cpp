#include "bb3d/core/Log.hpp"
#include "bb3d/core/Config.hpp"
#include "bb3d/core/Window.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/render/SwapChain.hpp"
#include "bb3d/render/Shader.hpp"
#include "bb3d/render/GraphicsPipeline.hpp"

#include <iostream>
#include <vector>

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
    BB_CORE_INFO("Test Unitaire 04 : Hello Triangle (Vulkan-Hpp)");

    try {
        bb3d::EngineConfig config;
        config.window.title = "BB3D - Hello Triangle";
        config.window.width = 800;
        config.window.height = 600;
        config.rasterizer.cullMode = "None";
        bb3d::Window window(config);

        bb3d::VulkanContext context;
#ifdef NDEBUG
        context.init(window.GetNativeWindow(), "Hello Triangle", false);
#else
        context.init(window.GetNativeWindow(), "Hello Triangle", true);
#endif
        bb3d::SwapChain swapChain(context, config.window.width, config.window.height);

        bb3d::Shader vertShader(context, "assets/shaders/triangle.vert.spv");
        bb3d::Shader fragShader(context, "assets/shaders/triangle.frag.spv");

        bb3d::GraphicsPipeline pipeline(context, swapChain, vertShader, fragShader, config, {}, false);

        vk::Device device = context.getDevice();
        vk::CommandPool commandPool = device.createCommandPool({ vk::CommandPoolCreateFlagBits::eResetCommandBuffer, context.getGraphicsQueueFamily() });

        // --- Frames In Flight ---
        const int MAX_FRAMES_IN_FLIGHT = 2;
        
        // --- Command Buffers ---
        std::vector<vk::CommandBuffer> commandBuffers = device.allocateCommandBuffers({ commandPool, vk::CommandBufferLevel::ePrimary, (uint32_t)MAX_FRAMES_IN_FLIGHT });

        // --- Sync Objects ---
        std::vector<vk::Semaphore> imageAvailableSemaphores(MAX_FRAMES_IN_FLIGHT);
        std::vector<vk::Semaphore> renderFinishedSemaphores(MAX_FRAMES_IN_FLIGHT);
        std::vector<vk::Fence> inFlightFences(MAX_FRAMES_IN_FLIGHT);

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            imageAvailableSemaphores[i] = device.createSemaphore({});
            renderFinishedSemaphores[i] = device.createSemaphore({});
            inFlightFences[i] = device.createFence({ vk::FenceCreateFlagBits::eSignaled });
        }

        uint32_t currentFrame = 0;

        BB_CORE_INFO("Démarrage de la boucle de rendu...");
        
        while (!window.ShouldClose()) {
            window.PollEvents();

            auto waitResult = device.waitForFences(1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
            device.resetFences(1, &inFlightFences[currentFrame]);

            uint32_t imageIndex;
            try {
                imageIndex = swapChain.acquireNextImage(imageAvailableSemaphores[currentFrame]);
            } catch (...) { continue; }

            auto& commandBuffer = commandBuffers[currentFrame];
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

            commandBuffer.draw(3, 1, 0, 0);
            commandBuffer.endRendering();

            transitionImageLayout(commandBuffer, image, swapChain.getImageFormat(), vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR);

            commandBuffer.end();

            vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
            vk::SubmitInfo submitInfo(1, &imageAvailableSemaphores[currentFrame], waitStages, 1, &commandBuffer, 1, &renderFinishedSemaphores[currentFrame]);

            context.getGraphicsQueue().submit(submitInfo, inFlightFences[currentFrame]);
            swapChain.present(renderFinishedSemaphores[currentFrame], imageIndex);

            currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        }

        device.waitIdle();
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            device.destroySemaphore(renderFinishedSemaphores[i]);
            device.destroySemaphore(imageAvailableSemaphores[i]);
            device.destroyFence(inFlightFences[i]);
        }
        device.destroyCommandPool(commandPool);
    } catch (const std::exception& e) {
        BB_CORE_ERROR("Erreur fatale : {}", e.what());
        return -1;
    }

    return 0;
}
