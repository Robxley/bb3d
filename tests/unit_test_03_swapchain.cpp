#include "bb3d/core/Log.hpp"
#include "bb3d/core/Config.hpp"
#include "bb3d/core/Window.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/render/SwapChain.hpp"

#include <iostream>
#include <vector>

int main() {
    bb3d::EngineConfig logConfig;
    logConfig.system.logDirectory = "unit_test_logs";
    bb3d::Log::Init(logConfig);
    BB_CORE_INFO("Test Unitaire 03 : SwapChain & Présentation (Vulkan-Hpp)");

    try {
        // 1. Config & Window
        bb3d::EngineConfig config;
        config.window.title = "BB3D - SwapChain Test";
        config.window.width = 800;
        config.window.height = 600;
        bb3d::Window window(config);

        // 2. Vulkan Context
        bb3d::VulkanContext context;
#ifdef NDEBUG
        context.init(window.GetNativeWindow(), "Test SwapChain", false);
#else
        context.init(window.GetNativeWindow(), "Test SwapChain", true);
#endif

        // 3. SwapChain
        bb3d::SwapChain swapChain(context, config.window.width, config.window.height);

        // 4. Sync Objects
        vk::Device device = context.getDevice();
        vk::Semaphore imageAvailableSemaphore = device.createSemaphore({});
        vk::Semaphore renderFinishedSemaphore = device.createSemaphore({});
        vk::Fence inFlightFence = device.createFence({ vk::FenceCreateFlagBits::eSignaled });

        BB_CORE_INFO("Démarrage de la boucle de rendu (100 frames)...");
        
        int frameCount = 0;
        while (!window.ShouldClose() && frameCount < 100) {
            window.PollEvents();

            auto waitResult = device.waitForFences(1, &inFlightFence, VK_TRUE, UINT64_MAX);
            device.resetFences(1, &inFlightFence);

            // 1. Acquire
            uint32_t imageIndex;
            try {
                imageIndex = swapChain.acquireNextImage(imageAvailableSemaphore);
            } catch (const std::exception& e) {
                BB_CORE_WARN("Erreur Acquire (Resize?): {}", e.what());
                break; 
            }

            // 2. Submit (Signal sync objects)
            vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
            vk::SubmitInfo submitInfo(1, &imageAvailableSemaphore, waitStages, 0, nullptr, 1, &renderFinishedSemaphore);

            context.getGraphicsQueue().submit(submitInfo, inFlightFence);

            // 3. Present
            swapChain.present(renderFinishedSemaphore, imageIndex);

            frameCount++;
            if (frameCount % 10 == 0) BB_CORE_INFO("Frame {}", frameCount);
        }

        device.waitIdle();

        // Cleanup Sync
        device.destroySemaphore(renderFinishedSemaphore);
        device.destroySemaphore(imageAvailableSemaphore);
        device.destroyFence(inFlightFence);

    } catch (const std::exception& e) {
        BB_CORE_ERROR("Erreur fatale : {}", e.what());
        return -1;
    }

    return 0;
}