#include "bb3d/core/Log.hpp"
#include "bb3d/core/Config.hpp"
#include "bb3d/core/Window.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/render/SwapChain.hpp"
#include "bb3d/render/Shader.hpp"
#include "bb3d/render/GraphicsPipeline.hpp"
#include "bb3d/render/Vertex.hpp"
#include "bb3d/render/Buffer.hpp"

#include <iostream>
#include <vector>

// Helper transition (identique au test 04)
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

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

int main() {
    bb3d::Log::Init();
    BB_CORE_INFO("Test Unitaire 05 : Vertex Buffer & VMA");

    try {
        bb3d::EngineConfig config;
        config.title = "BB3D - Vertex Buffer Quad";
        config.width = 800;
        config.height = 600;
        config.cullMode = "None"; // Fix pour l'ordre des sommets CW
        bb3d::Window window(config);

        bb3d::VulkanContext context;
#ifdef NDEBUG
        context.init(window.GetNativeWindow(), "Vertex Buffer Test", false);
#else
        context.init(window.GetNativeWindow(), "Vertex Buffer Test", true);
#endif
        bb3d::SwapChain swapChain(context, config.width, config.height);

        // --- Data : Un Quad (2 triangles) ---
        std::vector<bb3d::Vertex> vertices = {
            // Triangle 1
            {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // Top-left (Rouge)
            {{ 0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}}, // Top-right (Vert)
            {{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}}, // Bottom-right (Bleu)

            // Triangle 2
            {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // Top-left (Rouge)
            {{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}}, // Bottom-right (Bleu)
            {{-0.5f,  0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}, // Bottom-left (Blanc)
        };

        auto vertexBuffer = bb3d::Buffer::CreateVertexBuffer(context, vertices.data(), vertices.size() * sizeof(bb3d::Vertex));

        // --- Shaders ---
        bb3d::Shader vertShader(context, "assets/shaders/triangle_buffer.vert.spv");
        bb3d::Shader fragShader(context, "assets/shaders/triangle.frag.spv");

        // --- Pipeline ---
        bb3d::GraphicsPipeline pipeline(context, swapChain, vertShader, fragShader, config);

        // --- Infrastructure de commande ---
        VkCommandPool commandPool;
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = context.getGraphicsQueueFamily();
        vkCreateCommandPool(context.getDevice(), &poolInfo, nullptr, &commandPool);

        VkCommandBuffer commandBuffer;
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        vkAllocateCommandBuffers(context.getDevice(), &allocInfo, &commandBuffer);

        // --- Sync ---
        VkSemaphore imageAvailableSemaphore;
        VkSemaphore renderFinishedSemaphore;
        VkFence inFlightFence;
        VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT};
        vkCreateSemaphore(context.getDevice(), &semInfo, nullptr, &imageAvailableSemaphore);
        vkCreateSemaphore(context.getDevice(), &semInfo, nullptr, &renderFinishedSemaphore);
        vkCreateFence(context.getDevice(), &fenceInfo, nullptr, &inFlightFence);

        BB_CORE_INFO("Démarrage du rendu du Quad...");

        while (!window.ShouldClose()) {
            window.PollEvents();

            vkWaitForFences(context.getDevice(), 1, &inFlightFence, VK_TRUE, UINT64_MAX);
            vkResetFences(context.getDevice(), 1, &inFlightFence);

            uint32_t imageIndex = swapChain.acquireNextImage(imageAvailableSemaphore);

            vkResetCommandBuffer(commandBuffer, 0);
            VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            vkBeginCommandBuffer(commandBuffer, &beginInfo);

            VkImage image = swapChain.getImage(imageIndex);
            transitionImageLayout(commandBuffer, image, swapChain.getImageFormat(), 
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            VkRenderingAttachmentInfo colorAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
            colorAttachment.imageView = swapChain.getImageViews()[imageIndex];
            colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.clearValue.color = {{0.1f, 0.1f, 0.1f, 1.0f}};

            VkRenderingInfo renderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
            renderingInfo.renderArea = {{0, 0}, swapChain.getExtent()};
            renderingInfo.layerCount = 1;
            renderingInfo.colorAttachmentCount = 1;
            renderingInfo.pColorAttachments = &colorAttachment;

            vkCmdBeginRendering(commandBuffer, &renderingInfo);

            pipeline.bind(commandBuffer);
            
            // Viewport/Scissor
            VkViewport viewport{0.0f, 0.0f, (float)swapChain.getExtent().width, (float)swapChain.getExtent().height, 0.0f, 1.0f};
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            VkRect2D scissor{{0, 0}, swapChain.getExtent()};
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            // BIND VERTEX BUFFER
            VkBuffer vertexBuffers[] = {vertexBuffer->getHandle()};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

            // DRAW
            vkCmdDraw(commandBuffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);

            vkCmdEndRendering(commandBuffer);

            transitionImageLayout(commandBuffer, image, swapChain.getImageFormat(), 
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

            vkEndCommandBuffer(commandBuffer);

            VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            VkSemaphore waitSems[] = {imageAvailableSemaphore};
            VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = waitSems;
            submitInfo.pWaitDstStageMask = waitStages;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;
            VkSemaphore signalSems[] = {renderFinishedSemaphore};
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = signalSems;

            vkQueueSubmit(context.getGraphicsQueue(), 1, &submitInfo, inFlightFence);
            swapChain.present(renderFinishedSemaphore, imageIndex);
        }

        vkDeviceWaitIdle(context.getDevice());
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
