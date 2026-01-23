#include "bb3d/core/Log.hpp"
#include "bb3d/core/Config.hpp"
#include "bb3d/core/Window.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/render/SwapChain.hpp"

#include <iostream>
#include <vector>

// Helper pour la synchro
struct FrameSyncObjects {
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;
};

int main() {
    bb3d::Log::Init();
    BB_CORE_INFO("Test Unitaire 03 : SwapChain & Présentation");

    try {
        // 1. Config & Window
        bb3d::EngineConfig config;
        config.title = "BB3D - SwapChain Test";
        config.width = 800;
        config.height = 600;
        bb3d::Window window(config);

        // 2. Vulkan Context
        bb3d::VulkanContext context;
#ifdef NDEBUG
        context.init(window.GetNativeWindow(), "Test SwapChain", false);
#else
        context.init(window.GetNativeWindow(), "Test SwapChain", true);
#endif

        // 3. SwapChain
        bb3d::SwapChain swapChain(context, config.width, config.height);

        // 4. Sync Objects (Pour 1 frame en vol pour simplifier le test)
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkSemaphore imageAvailableSemaphore;
        VkSemaphore renderFinishedSemaphore;
        VkFence inFlightFence;

        VkDevice device = context.getDevice();
        vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore);
        vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore);
        vkCreateFence(device, &fenceInfo, nullptr, &inFlightFence);

        BB_CORE_INFO("Démarrage de la boucle de rendu (100 frames)...");
        
        int frameCount = 0;
        while (!window.ShouldClose() && frameCount < 100) {
            window.PollEvents();

            // Attendre la frame précédente
            vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
            vkResetFences(device, 1, &inFlightFence);

            // 1. Acquire
            uint32_t imageIndex;
            try {
                imageIndex = swapChain.acquireNextImage(imageAvailableSemaphore);
            } catch (const std::runtime_error& e) {
                // Resize non géré dans ce test simple
                BB_CORE_WARN("Erreur Acquire (Resize?): {}", e.what());
                break; 
            }

            // 2. Submit (Vide pour l'instant, juste pour la synchro)
            // On doit quand même submit quelque chose pour signaler le renderFinishedSemaphore
            // Sinon vkQueuePresentKHR va attendre indéfiniment.
            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

            VkSemaphore waitSemaphores[] = {imageAvailableSemaphore};
            VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = waitSemaphores;
            submitInfo.pWaitDstStageMask = waitStages;

            submitInfo.commandBufferCount = 0; // Pas de command buffer
            submitInfo.pCommandBuffers = nullptr;

            VkSemaphore signalSemaphores[] = {renderFinishedSemaphore};
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = signalSemaphores;

            if (vkQueueSubmit(context.getGraphicsQueue(), 1, &submitInfo, inFlightFence) != VK_SUCCESS) {
                throw std::runtime_error("Failed to submit draw command buffer!");
            }

            // 3. Present
            swapChain.present(renderFinishedSemaphore, imageIndex);

            frameCount++;
            if (frameCount % 10 == 0) BB_CORE_INFO("Frame {}", frameCount);
        }

        vkDeviceWaitIdle(device);

        // Cleanup Sync
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
        vkDestroyFence(device, inFlightFence, nullptr);

    } catch (const std::exception& e) {
        BB_CORE_ERROR("Erreur fatale : {}", e.what());
        return -1;
    }

    return 0;
}
