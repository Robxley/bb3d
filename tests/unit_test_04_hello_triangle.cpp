#include "bb3d/core/Log.hpp"
#include "bb3d/core/Config.hpp"
#include "bb3d/core/Window.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/render/SwapChain.hpp"
#include "bb3d/render/Shader.hpp"
#include "bb3d/render/GraphicsPipeline.hpp"

#include <iostream>
#include <vector>

void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = 0;
        sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
}

int main() {
    bb3d::Log::Init();
    BB_CORE_INFO("Test Unitaire 04 : Hello Triangle (Dynamic Rendering)");

    try {
        bb3d::EngineConfig config;
        config.title = "BB3D - Hello Triangle";
        config.width = 800;
        config.height = 600;
        config.cullMode = "None"; // Désactiver le culling pour voir le triangle
        bb3d::Window window(config);

        bb3d::VulkanContext context;
#ifdef NDEBUG
        context.init(window.GetNativeWindow(), "Hello Triangle", false);
#else
        context.init(window.GetNativeWindow(), "Hello Triangle", true);
#endif
        bb3d::SwapChain swapChain(context, config.width, config.height);

        // --- Shaders ---
        // Les shaders doivent être dans le dossier assets/shaders relatif à l'exécutable
        bb3d::Shader vertShader(context, "assets/shaders/triangle.vert.spv");
        bb3d::Shader fragShader(context, "assets/shaders/triangle.frag.spv");

        // --- Pipeline ---
        // On passe {} pour descriptorSetLayouts et false pour useVertexInput car les sommets sont hardcodés
        bb3d::GraphicsPipeline pipeline(context, swapChain, vertShader, fragShader, config, {}, false);

        // --- Command Pool ---
        VkCommandPool commandPool;
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = context.getGraphicsQueueFamily();

        if (vkCreateCommandPool(context.getDevice(), &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }

        // --- Command Buffer ---
        VkCommandBuffer commandBuffer;
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(context.getDevice(), &allocInfo, &commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }

        // --- Sync Objects ---
        VkSemaphore imageAvailableSemaphore;
        VkSemaphore renderFinishedSemaphore;
        VkFence inFlightFence;

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        vkCreateSemaphore(context.getDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphore);
        vkCreateSemaphore(context.getDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphore);
        vkCreateFence(context.getDevice(), &fenceInfo, nullptr, &inFlightFence);


        BB_CORE_INFO("Démarrage de la boucle de rendu...");
        
        while (!window.ShouldClose()) {
            window.PollEvents();

            vkWaitForFences(context.getDevice(), 1, &inFlightFence, VK_TRUE, UINT64_MAX);
            vkResetFences(context.getDevice(), 1, &inFlightFence);

            uint32_t imageIndex;
            try {
                imageIndex = swapChain.acquireNextImage(imageAvailableSemaphore);
            } catch (const std::runtime_error& e) {
                 // Resize ignoré pour ce test
                 continue;
            }

            // --- Recording ---
            vkResetCommandBuffer(commandBuffer, 0);
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

            if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
                throw std::runtime_error("failed to begin recording command buffer!");
            }

            // 1. Transition: Undefined -> Color Attachment
            VkImage image = swapChain.getImage(imageIndex);
            transitionImageLayout(commandBuffer, image, swapChain.getImageFormat(), 
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            // 2. Begin Rendering (Dynamic Rendering)
            VkRenderingAttachmentInfo colorAttachment{};
            colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            colorAttachment.imageView = swapChain.getImageViews()[imageIndex];
            colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

            VkRenderingInfo renderingInfo{};
            renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            renderingInfo.renderArea = {{0, 0}, swapChain.getExtent()};
            renderingInfo.layerCount = 1;
            renderingInfo.colorAttachmentCount = 1;
            renderingInfo.pColorAttachments = &colorAttachment;

            vkCmdBeginRendering(commandBuffer, &renderingInfo);

            // 3. Draw
            pipeline.bind(commandBuffer);
            
            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = (float)swapChain.getExtent().width;
            viewport.height = (float)swapChain.getExtent().height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = swapChain.getExtent();
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            vkCmdDraw(commandBuffer, 3, 1, 0, 0);

            vkCmdEndRendering(commandBuffer);

            // 4. Transition Color Attachment -> Present
            transitionImageLayout(commandBuffer, image, swapChain.getImageFormat(), 
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

            if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
                throw std::runtime_error("failed to record command buffer!");
            }

            // --- Submit ---
            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

            VkSemaphore waitSemaphores[] = {imageAvailableSemaphore};
            VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = waitSemaphores;
            submitInfo.pWaitDstStageMask = waitStages;

            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;

            VkSemaphore signalSemaphores[] = {renderFinishedSemaphore};
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = signalSemaphores;

            if (vkQueueSubmit(context.getGraphicsQueue(), 1, &submitInfo, inFlightFence) != VK_SUCCESS) {
                throw std::runtime_error("failed to submit draw command buffer!");
            }

            // --- Present ---
            swapChain.present(renderFinishedSemaphore, imageIndex);
        }

        vkDeviceWaitIdle(context.getDevice());

        // Cleanup
        vkDestroySemaphore(context.getDevice(), renderFinishedSemaphore, nullptr);
        vkDestroySemaphore(context.getDevice(), imageAvailableSemaphore, nullptr);
        vkDestroyFence(context.getDevice(), inFlightFence, nullptr);
        vkDestroyCommandPool(context.getDevice(), commandPool, nullptr);
    } catch (const std::exception& e) {
        BB_CORE_ERROR("Erreur fatale : {}", e.what());
        return -1;
    }

    return 0;
}
