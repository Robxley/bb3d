#include "bb3d/render/Renderer.hpp"
#include "bb3d/core/Window.hpp"
#include "bb3d/core/Log.hpp"
#include "bb3d/render/UniformBuffer.hpp"
#include "bb3d/scene/Components.hpp"
#include <array>

namespace bb3d {

Renderer::Renderer(VulkanContext& context, Window& window, const EngineConfig& config)
    : m_context(context), m_window(window) {
    
    m_swapChain = CreateScope<SwapChain>(context, config.window.width, config.window.height);
    
    createSyncObjects();
    createGlobalDescriptors();

    // Chargement shaders par défaut
    m_defaultVert = CreateScope<Shader>(context, "assets/shaders/simple_3d.vert.spv");
    m_defaultFrag = CreateScope<Shader>(context, "assets/shaders/simple_3d.frag.spv");
    
    vk::PushConstantRange pushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4));
    m_defaultPipeline = CreateScope<GraphicsPipeline>(context, *m_swapChain, *m_defaultVert, *m_defaultFrag, config, std::vector<vk::DescriptorSetLayout>{m_globalDescriptorLayout}, std::vector<vk::PushConstantRange>{pushConstantRange});
}

Renderer::~Renderer() {
    auto dev = m_context.getDevice();
    if (dev) {
        dev.waitIdle();

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (m_imageAvailableSemaphores[i]) dev.destroySemaphore(m_imageAvailableSemaphores[i]);
            if (m_renderFinishedSemaphores[i]) dev.destroySemaphore(m_renderFinishedSemaphores[i]);
            if (m_inFlightFences[i]) dev.destroyFence(m_inFlightFences[i]);
        }

        if (m_descriptorPool) dev.destroyDescriptorPool(m_descriptorPool);
        if (m_globalDescriptorLayout) dev.destroyDescriptorSetLayout(m_globalDescriptorLayout);
        if (m_commandPool) dev.destroyCommandPool(m_commandPool);
    }
}

void Renderer::createSyncObjects() {
    auto dev = m_context.getDevice();
    const uint32_t imageCount = static_cast<uint32_t>(m_swapChain->getImageCount());
    m_imageAvailableSemaphores.resize(imageCount);
    m_renderFinishedSemaphores.resize(imageCount);
    m_inFlightFences.resize(imageCount);

    for (size_t i = 0; i < imageCount; i++) {
        m_imageAvailableSemaphores[i] = dev.createSemaphore({});
        m_renderFinishedSemaphores[i] = dev.createSemaphore({});
        m_inFlightFences[i] = dev.createFence({ vk::FenceCreateFlagBits::eSignaled });
    }

    m_commandPool = dev.createCommandPool({ vk::CommandPoolCreateFlagBits::eResetCommandBuffer, m_context.getGraphicsQueueFamily() });
    m_commandBuffers = dev.allocateCommandBuffers({ m_commandPool, vk::CommandBufferLevel::ePrimary, imageCount });
}

void Renderer::createGlobalDescriptors() {
    auto dev = m_context.getDevice();
    const uint32_t imageCount = static_cast<uint32_t>(m_swapChain->getImageCount());
    
    m_cameraUbo = CreateScope<UniformBuffer>(m_context, sizeof(GlobalUBO));

    vk::DescriptorSetLayoutBinding cameraBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex);
    m_globalDescriptorLayout = dev.createDescriptorSetLayout({ {}, 1, &cameraBinding });

    vk::DescriptorPoolSize poolSize(vk::DescriptorType::eUniformBuffer, imageCount);
    m_descriptorPool = dev.createDescriptorPool({ {}, imageCount, 1, &poolSize });

    std::vector<vk::DescriptorSetLayout> layouts(imageCount, m_globalDescriptorLayout);
    m_globalDescriptorSets = dev.allocateDescriptorSets({ m_descriptorPool, imageCount, layouts.data() });

    for (size_t i = 0; i < imageCount; i++) {
        vk::DescriptorBufferInfo bufferInfo(m_cameraUbo->getHandle(), 0, sizeof(GlobalUBO));
        vk::WriteDescriptorSet write(m_globalDescriptorSets[i], 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &bufferInfo);
        dev.updateDescriptorSets(1, &write, 0, nullptr);
    }
}

void Renderer::render(Scene& scene) {
    auto dev = m_context.getDevice();
    const uint32_t imageCount = static_cast<uint32_t>(m_swapChain->getImageCount());
    
    // Attendre la frame
    [[maybe_unused]] auto waitRes = dev.waitForFences(1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);
    
    // Chercher la caméra active
    Camera* activeCamera = nullptr;
    auto camView = scene.getRegistry().view<CameraComponent>();
    for (auto entity : camView) {
        auto& comp = camView.get<CameraComponent>(entity);
        if (comp.active) {
            activeCamera = comp.camera.get();
            break;
        }
    }
    if (!activeCamera) return;

    // Mettre à jour UBO global
    GlobalUBO uboData;
    uboData.view = activeCamera->getViewMatrix();
    uboData.proj = activeCamera->getProjectionMatrix();
    m_cameraUbo->update(&uboData, sizeof(uboData));

    uint32_t imageIndex;
    try {
        imageIndex = m_swapChain->acquireNextImage(m_imageAvailableSemaphores[m_currentFrame]);
    } catch (...) { return; }

    dev.resetFences(1, &m_inFlightFences[m_currentFrame]);

    auto& cb = m_commandBuffers[m_currentFrame];
    cb.reset({});
    cb.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

    // --- TRANSITIONS ---
    vk::Image colorImage = m_swapChain->getImage(imageIndex);
    vk::Image depthImage = m_swapChain->getDepthImage();

    // Color: Undefined -> Attachment
    vk::ImageMemoryBarrier colorBarrier({}, vk::AccessFlagBits::eColorAttachmentWrite, 
        vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, 
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, colorImage, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
    
    // Depth: Undefined -> Attachment
    vk::ImageMemoryBarrier depthBarrier({}, vk::AccessFlagBits::eDepthStencilAttachmentWrite, 
        vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal, 
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, depthImage, { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 });

    cb.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests, 
        {}, nullptr, nullptr, { colorBarrier, depthBarrier });

    // --- RENDERING ---
    vk::RenderingAttachmentInfo colorAttr(m_swapChain->getImageViews()[imageIndex], vk::ImageLayout::eColorAttachmentOptimal, vk::ResolveModeFlagBits::eNone, {}, vk::ImageLayout::eUndefined, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::ClearValue(vk::ClearColorValue(std::array<float, 4>{0.1f, 0.1f, 0.1f, 1.0f})));
    vk::RenderingAttachmentInfo depthAttr(m_swapChain->getDepthImageView(), vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::ResolveModeFlagBits::eNone, {}, vk::ImageLayout::eUndefined, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::ClearValue(vk::ClearDepthStencilValue(1.0f, 0)));
    vk::RenderingInfo renderInfo({}, {{0, 0}, m_swapChain->getExtent()}, 1, 0, 1, &colorAttr, &depthAttr);

    cb.beginRendering(renderInfo);
    m_defaultPipeline->bind(cb);
    cb.setViewport(0, vk::Viewport(0, 0, (float)m_swapChain->getExtent().width, (float)m_swapChain->getExtent().height, 0, 1));
    cb.setScissor(0, vk::Rect2D({0, 0}, m_swapChain->getExtent()));
    cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_defaultPipeline->getLayout(), 0, 1, &m_globalDescriptorSets[m_currentFrame], 0, nullptr);

    // Dessiner toutes les entités avec MeshComponent
    auto meshView = scene.getRegistry().view<MeshComponent, TransformComponent>();
    for (auto entity : meshView) {
        auto [meshComp, transformComp] = meshView.get<MeshComponent, TransformComponent>(entity);
        if (meshComp.mesh) {
            glm::mat4 modelMatrix = transformComp.getTransform();
            cb.pushConstants(m_defaultPipeline->getLayout(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4), &modelMatrix);
            meshComp.mesh->draw(cb);
        }
    }

    cb.endRendering();

    // Transition Color: Attachment -> Present
    vk::ImageMemoryBarrier presentBarrier(vk::AccessFlagBits::eColorAttachmentWrite, {}, 
        vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR, 
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, colorImage, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

    cb.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eBottomOfPipe, {}, nullptr, nullptr, presentBarrier);

    cb.end();

    vk::PipelineStageFlags waitStages = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    m_context.getGraphicsQueue().submit(vk::SubmitInfo(1, &m_imageAvailableSemaphores[m_currentFrame], &waitStages, 1, &cb, 1, &m_renderFinishedSemaphores[m_currentFrame]), m_inFlightFences[m_currentFrame]);

    m_swapChain->present(m_renderFinishedSemaphores[m_currentFrame], imageIndex);
    m_currentFrame = (m_currentFrame + 1) % imageCount;
}

void Renderer::onResize(int width, int height) {
    m_swapChain->recreate(width, height);
}

} // namespace bb3d