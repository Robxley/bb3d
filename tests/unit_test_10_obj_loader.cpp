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

// Helper transition
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

// Helper RAII
struct TestResources {
    VkDevice device;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    VkFence inFlightFence = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;

    TestResources(VkDevice dev) : device(dev) {}

    ~TestResources() {
        if (descriptorPool) vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        if (descriptorSetLayout) vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        for (auto s : renderFinishedSemaphores) vkDestroySemaphore(device, s, nullptr);
        for (auto s : imageAvailableSemaphores) vkDestroySemaphore(device, s, nullptr);
        if (inFlightFence) vkDestroyFence(device, inFlightFence, nullptr);
        if (commandPool) vkDestroyCommandPool(device, commandPool, nullptr);
    }
};

int main() {
    bb3d::Log::Init();
    BB_CORE_INFO("Test Unitaire 10 : Rendu Mesh OBJ (House)");

    try {
        bb3d::EngineConfig config;
        config.title = "BB3D - OBJ Loader (House)";
        config.width = 1280;
        config.height = 720;
        config.cullMode = "None"; 
        bb3d::Window window(config);

        bb3d::VulkanContext context;
#ifdef NDEBUG
        context.init(window.GetNativeWindow(), "OBJ Test", false);
#else
        context.init(window.GetNativeWindow(), "OBJ Test", true);
#endif
        TestResources res(context.getDevice());

        bb3d::SwapChain swapChain(context, config.width, config.height);
        
        bb3d::JobSystem jobSystem;
        jobSystem.init();
        bb3d::ResourceManager resources(context, jobSystem);

        // Charger le modèle Maison (OBJ)
        auto model = resources.load<bb3d::Model>("assets/models/house.obj");
        auto texture = resources.load<bb3d::Texture>("assets/textures/Bricks092_1K-JPG_Color.jpg");

        // --- Auto-Fit Camera ---
        auto bounds = model->getBounds();
        glm::vec3 boundsCenter = bounds.center();
        glm::vec3 boundsSize = bounds.size();
        float maxDim = std::max(std::max(boundsSize.x, boundsSize.y), boundsSize.z);
        
        float fov = 45.0f;
        float distance = (maxDim) / std::tan(glm::radians(fov) * 0.5f);
        distance *= 1.5f;

        BB_CORE_INFO("Model Bounds: Size({:.2f}, {:.2f}, {:.2f})", boundsSize.x, boundsSize.y, boundsSize.z);
        BB_CORE_INFO("Auto-Camera Distance: {:.2f}", distance);

        // --- UBO ---
        bb3d::UniformBuffer ubo(context, sizeof(UniformBufferObject));

        // --- Descriptor Set Layout ---
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutBinding samplerLayoutBinding{};
        samplerLayoutBinding.binding = 1;
        samplerLayoutBinding.descriptorCount = 1;
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.pImmutableSamplers = nullptr;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};
        VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        vkCreateDescriptorSetLayout(context.getDevice(), &layoutInfo, nullptr, &res.descriptorSetLayout);

        // --- Descriptor Pool ---
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = 1;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = 1;
        
        vkCreateDescriptorPool(context.getDevice(), &poolInfo, nullptr, &res.descriptorPool);

        // --- Descriptor Set ---
        VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocInfo.descriptorPool = res.descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &res.descriptorSetLayout;
        VkDescriptorSet descriptorSet;
        vkAllocateDescriptorSets(context.getDevice(), &allocInfo, &descriptorSet);

        // --- Update Descriptors ---
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = ubo.getHandle();
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = texture->getImageView();
        imageInfo.sampler = texture->getSampler();

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSet;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSet;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(context.getDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

        // --- Pipeline (Textured) ---
        bb3d::Shader vertShader(context, "assets/shaders/textured_mesh.vert.spv");
        bb3d::Shader fragShader(context, "assets/shaders/textured_mesh.frag.spv");
        bb3d::GraphicsPipeline pipeline(context, swapChain, vertShader, fragShader, config, {res.descriptorSetLayout});

        // --- Commands ---
        VkCommandPoolCreateInfo cmdPoolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cmdPoolInfo.queueFamilyIndex = context.getGraphicsQueueFamily();
        vkCreateCommandPool(context.getDevice(), &cmdPoolInfo, nullptr, &res.commandPool);

        VkCommandBuffer commandBuffer;
        VkCommandBufferAllocateInfo cmdAllocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cmdAllocInfo.commandPool = res.commandPool;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAllocInfo.commandBufferCount = 1;
        vkAllocateCommandBuffers(context.getDevice(), &cmdAllocInfo, &commandBuffer);

        // --- Sync ---
        uint32_t imageCount = swapChain.getImageCount();
        res.imageAvailableSemaphores.resize(imageCount);
        res.renderFinishedSemaphores.resize(imageCount);
        VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT};
        for (uint32_t i = 0; i < imageCount; ++i) {
            vkCreateSemaphore(context.getDevice(), &semInfo, nullptr, &res.imageAvailableSemaphores[i]);
            vkCreateSemaphore(context.getDevice(), &semInfo, nullptr, &res.renderFinishedSemaphores[i]);
        }
        vkCreateFence(context.getDevice(), &fenceInfo, nullptr, &res.inFlightFence);

        bb3d::Camera camera(fov, (float)config.width / (float)config.height, 0.1f, distance * 10.0f);
        camera.setPosition({0.0f, maxDim * 0.5f, distance});
        camera.lookAt({0.0f, 0.0f, 0.0f});

        BB_CORE_INFO("Démarrage du rendu...");

        while (!window.ShouldClose()) {
            window.PollEvents();

            UniformBufferObject uboData{};
            static auto startTime = std::chrono::high_resolution_clock::now();
            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
            
            glm::mat4 modelMat = glm::rotate(glm::mat4(1.0f), time * glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            modelMat = glm::translate(modelMat, -boundsCenter);
            
            uboData.model = modelMat;
            uboData.view = camera.getViewMatrix();
            uboData.proj = camera.getProjectionMatrix();
            ubo.update(&uboData, sizeof(uboData));

            vkWaitForFences(context.getDevice(), 1, &res.inFlightFence, VK_TRUE, UINT64_MAX);
            vkResetFences(context.getDevice(), 1, &res.inFlightFence);

            static uint32_t currentFrame = 0;
            uint32_t imageIndex = swapChain.acquireNextImage(res.imageAvailableSemaphores[currentFrame]);

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
            colorAttachment.clearValue.color = {{0.1f, 0.1f, 0.1f, 1.0f}};

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

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getLayout(), 0, 1, &descriptorSet, 0, nullptr);

            if (model) {
                model->draw(commandBuffer);
            }

            vkCmdEndRendering(commandBuffer);

            transitionImageLayout(commandBuffer, image, swapChain.getImageFormat(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

            vkEndCommandBuffer(commandBuffer);

            VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            VkSemaphore waitSems[] = {res.imageAvailableSemaphores[currentFrame]};
            VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = waitSems;
            submitInfo.pWaitDstStageMask = waitStages;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;
            VkSemaphore signalSems[] = {res.renderFinishedSemaphores[currentFrame]};
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = signalSems;

            vkQueueSubmit(context.getGraphicsQueue(), 1, &submitInfo, res.inFlightFence);
            swapChain.present(res.renderFinishedSemaphores[currentFrame], imageIndex);

            currentFrame = (currentFrame + 1) % imageCount;
        }

        vkDeviceWaitIdle(context.getDevice());
        jobSystem.shutdown();

    } catch (const std::exception& e) {
        BB_CORE_ERROR("Erreur fatale : {}", e.what());
        return -1;
    }

    return 0;
}