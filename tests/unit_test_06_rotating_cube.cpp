#include "bb3d/core/Log.hpp"
#include "bb3d/core/Config.hpp"
#include "bb3d/core/Window.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/render/SwapChain.hpp"
#include "bb3d/render/Shader.hpp"
#include "bb3d/render/GraphicsPipeline.hpp"
#include "bb3d/render/Vertex.hpp"
#include "bb3d/render/Buffer.hpp"
#include "bb3d/render/UniformBuffer.hpp"
#include "bb3d/scene/Camera.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>
#include <chrono>

// Helper transition (toujours le même)
void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
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

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

int main() {
    bb3d::Log::Init();
    BB_CORE_INFO("Test Unitaire 06 : Rotating Cube (UBO & Descriptor Sets)");

    try {
        bb3d::EngineConfig config;
        config.title = "BB3D - Rotating Cube";
        config.width = 800;
        config.height = 600;
        bb3d::Window window(config);

        bb3d::VulkanContext context;
#ifdef NDEBUG
        context.init(window.GetNativeWindow(), "Cube Test", false);
#else
        context.init(window.GetNativeWindow(), "Cube Test", true);
#endif
        bb3d::SwapChain swapChain(context, config.width, config.height);

        // --- Cube Data ---
        std::vector<bb3d::Vertex> vertices = {
            // Front face (Z=0.5)
            {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
            {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
            {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
            {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
            {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
            {{-0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},

            // Back face (Z=-0.5)
            {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
            {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
            {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
            {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
            {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
            {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},

            // Left face (X=-0.5)
            {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
            {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
            {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
            {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
            {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
            {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},

            // Right face (X=0.5)
            {{ 0.5f, -0.5f,  0.5f}, {1.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
            {{ 0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
            {{ 0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
            {{ 0.5f, -0.5f,  0.5f}, {1.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
            {{ 0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
            {{ 0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},

            // Top face (Y=-0.5)
            {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
            {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
            {{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
            {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
            {{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
            {{-0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

            // Bottom face (Y=0.5)
            {{-0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
            {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
            {{ 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
            {{-0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
            {{ 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
            {{-0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
        };

        auto vertexBuffer = bb3d::Buffer::CreateVertexBuffer(context, vertices.data(), vertices.size() * sizeof(bb3d::Vertex));

        // --- UBO ---
        bb3d::UniformBuffer ubo(context, sizeof(UniformBufferObject));

        // --- Descriptor Set Layout ---
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        uboLayoutBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &uboLayoutBinding;

        VkDescriptorSetLayout descriptorSetLayout;
        vkCreateDescriptorSetLayout(context.getDevice(), &layoutInfo, nullptr, &descriptorSetLayout);

        // --- Descriptor Pool ---
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = 1;

        VkDescriptorPool descriptorPool;
        vkCreateDescriptorPool(context.getDevice(), &poolInfo, nullptr, &descriptorPool);

        // --- Descriptor Set ---
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &descriptorSetLayout;

        VkDescriptorSet descriptorSet;
        vkAllocateDescriptorSets(context.getDevice(), &allocInfo, &descriptorSet);

        // --- Update Descriptor Set ---
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = ubo.getHandle();
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(context.getDevice(), 1, &descriptorWrite, 0, nullptr);

        // --- Pipeline ---
        bb3d::Shader vertShader(context, "assets/shaders/simple_3d.vert.spv");
        bb3d::Shader fragShader(context, "assets/shaders/simple_3d.frag.spv");
        bb3d::GraphicsPipeline pipeline(context, swapChain, vertShader, fragShader, config, {descriptorSetLayout});

        // --- Commands & Sync ---
        // (Copie collé du test précédent pour l'infra)
        VkCommandPool commandPool;
        VkCommandPoolCreateInfo cmdPoolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cmdPoolInfo.queueFamilyIndex = context.getGraphicsQueueFamily();
        vkCreateCommandPool(context.getDevice(), &cmdPoolInfo, nullptr, &commandPool);

        VkCommandBuffer commandBuffer;
        VkCommandBufferAllocateInfo cmdAllocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cmdAllocInfo.commandPool = commandPool;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAllocInfo.commandBufferCount = 1;
        vkAllocateCommandBuffers(context.getDevice(), &cmdAllocInfo, &commandBuffer);

        VkSemaphore imageAvailableSemaphore, renderFinishedSemaphore;
        VkFence inFlightFence;
        VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT};
        vkCreateSemaphore(context.getDevice(), &semInfo, nullptr, &imageAvailableSemaphore);
        vkCreateSemaphore(context.getDevice(), &semInfo, nullptr, &renderFinishedSemaphore);
        vkCreateFence(context.getDevice(), &fenceInfo, nullptr, &inFlightFence);

        // --- Camera ---
        bb3d::Camera camera(45.0f, (float)config.width / (float)config.height, 0.1f, 100.0f);
        camera.setPosition({0.0f, 0.0f, 3.0f});
        camera.lookAt({0.0f, 0.0f, 0.0f});

        BB_CORE_INFO("Démarrage du rendu 3D...");
        auto startTime = std::chrono::high_resolution_clock::now();

        while (!window.ShouldClose()) {
            window.PollEvents();

            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

            // Update UBO
            UniformBufferObject uboData{};
            uboData.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            uboData.view = camera.getViewMatrix();
            uboData.proj = camera.getProjectionMatrix();
            ubo.update(&uboData, sizeof(uboData));

            vkWaitForFences(context.getDevice(), 1, &inFlightFence, VK_TRUE, UINT64_MAX);
            vkResetFences(context.getDevice(), 1, &inFlightFence);

            uint32_t imageIndex = swapChain.acquireNextImage(imageAvailableSemaphore);

            vkResetCommandBuffer(commandBuffer, 0);
            VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            vkBeginCommandBuffer(commandBuffer, &beginInfo);

            VkImage image = swapChain.getImage(imageIndex);
            transitionImageLayout(commandBuffer, image, swapChain.getImageFormat(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            VkRenderingAttachmentInfo colorAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
            colorAttachment.imageView = swapChain.getImageViews()[imageIndex];
            colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

            VkRenderingInfo renderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
            renderingInfo.renderArea = {{0, 0}, swapChain.getExtent()};
            renderingInfo.layerCount = 1;
            renderingInfo.colorAttachmentCount = 1;
            renderingInfo.pColorAttachments = &colorAttachment;

            vkCmdBeginRendering(commandBuffer, &renderingInfo);

            pipeline.bind(commandBuffer);
            
            VkViewport viewport{0.0f, 0.0f, (float)swapChain.getExtent().width, (float)swapChain.getExtent().height, 0.0f, 1.0f};
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            VkRect2D scissor{{0, 0}, swapChain.getExtent()};
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            VkBuffer vertexBuffers[] = {vertexBuffer->getHandle()};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            
            // BIND DESCRIPTOR SET
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getLayout(), 0, 1, &descriptorSet, 0, nullptr);

            vkCmdDraw(commandBuffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);

            vkCmdEndRendering(commandBuffer);

            transitionImageLayout(commandBuffer, image, swapChain.getImageFormat(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

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
        // Cleanup
        vkDestroyDescriptorPool(context.getDevice(), descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(context.getDevice(), descriptorSetLayout, nullptr);
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
