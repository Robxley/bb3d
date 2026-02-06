#include "bb3d/render/Renderer.hpp"
#include "bb3d/core/Window.hpp"
#include "bb3d/core/Log.hpp"
#include "bb3d/render/UniformBuffer.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/render/MeshGenerator.hpp"
#include <array>
#include <algorithm>
#include <SDL3/SDL.h>

namespace bb3d {

Renderer::Renderer(VulkanContext& context, Window& window, JobSystem& jobSystem, const EngineConfig& config)
    : m_context(context), m_window(window), m_jobSystem(jobSystem), m_config(config) {
    m_swapChain = CreateScope<SwapChain>(context, config.window.width, config.window.height);
    
    m_renderCommands.reserve(1000);
    m_instanceTransforms.reserve(MAX_INSTANCES);

    if (m_config.graphics.enableOffscreenRendering) {
        uint32_t w = static_cast<uint32_t>(m_swapChain->getExtent().width * m_config.graphics.renderScale);
        uint32_t h = static_cast<uint32_t>(m_swapChain->getExtent().height * m_config.graphics.renderScale);
        w = std::max(1u, w); h = std::max(1u, h);
        m_renderTarget = CreateScope<RenderTarget>(context, w, h);
        BB_CORE_INFO("Renderer: Offscreen Rendering Enabled (Resolution: {0}x{1})", w, h);
    }

    createSyncObjects();
    createGlobalDescriptors();
    createPipelines(config);
    createCopyPipeline();

    m_skyboxCube = MeshGenerator::createCube(m_context, 1.0f); 
    m_internalSkyboxMat = CreateRef<SkyboxMaterial>(m_context);
    m_internalSkySphereMat = CreateRef<SkySphereMaterial>(m_context);
    m_fallbackMaterial = CreateRef<PBRMaterial>(m_context);
}

Renderer::~Renderer() {
    auto dev = m_context.getDevice();
    if (dev) {
        try { dev.waitIdle(); } catch(...) {}
        
        m_renderCommands.clear();
        m_instanceTransforms.clear();
        m_defaultMaterials.clear();
        m_pipelines.clear();
        m_shaders.clear();
        
        m_internalSkyboxMat.reset();
        m_internalSkySphereMat.reset();
        m_fallbackMaterial.reset();
        m_skyboxCube.reset();
        m_cameraUbos.clear();
        m_instanceBuffers.clear();

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (m_imageAvailableSemaphores[i]) dev.destroySemaphore(m_imageAvailableSemaphores[i]);
            if (m_inFlightFences[i]) dev.destroyFence(m_inFlightFences[i]);
        }
        for (auto semaphore : m_renderFinishedSemaphores) if (semaphore) dev.destroySemaphore(semaphore);
        
        if (m_descriptorPool) dev.destroyDescriptorPool(m_descriptorPool);
        if (m_globalDescriptorLayout) dev.destroyDescriptorSetLayout(m_globalDescriptorLayout);
        if (m_copyLayout) dev.destroyDescriptorSetLayout(m_copyLayout);
        for (auto& [type, layout] : m_layouts) if(layout) dev.destroyDescriptorSetLayout(layout);
        if (m_commandPool) dev.destroyCommandPool(m_commandPool);
    }
}

void Renderer::createSyncObjects() {
    auto dev = m_context.getDevice();
    m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        m_imageAvailableSemaphores[i] = dev.createSemaphore({});
        m_inFlightFences[i] = dev.createFence({ vk::FenceCreateFlagBits::eSignaled });
    }
    m_renderFinishedSemaphores.resize(m_swapChain->getImageCount());
    for (size_t i = 0; i < m_renderFinishedSemaphores.size(); i++) m_renderFinishedSemaphores[i] = dev.createSemaphore({});
    m_commandPool = dev.createCommandPool({ vk::CommandPoolCreateFlagBits::eResetCommandBuffer, m_context.getGraphicsQueueFamily() });
    m_commandBuffers = dev.allocateCommandBuffers({ m_commandPool, vk::CommandBufferLevel::ePrimary, MAX_FRAMES_IN_FLIGHT });
    m_imagesInUseFences.resize(m_swapChain->getImageCount(), nullptr);
}

void Renderer::createGlobalDescriptors() {
    auto dev = m_context.getDevice();
    m_cameraUbos.resize(MAX_FRAMES_IN_FLIGHT);
    m_instanceBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_cameraUbos[i] = CreateScope<UniformBuffer>(m_context, sizeof(GlobalUBO));
        m_instanceBuffers[i] = CreateScope<Buffer>(m_context, sizeof(glm::mat4) * MAX_INSTANCES, vk::BufferUsageFlagBits::eStorageBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT);
    }
    std::vector<vk::DescriptorSetLayoutBinding> bindings = {
        {0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment},
        {1, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eVertex}
    };
    m_globalDescriptorLayout = dev.createDescriptorSetLayout({ {}, (uint32_t)bindings.size(), bindings.data() });
    std::vector<vk::DescriptorPoolSize> pSizes = { {vk::DescriptorType::eUniformBuffer, 500}, {vk::DescriptorType::eCombinedImageSampler, 1000}, {vk::DescriptorType::eStorageBuffer, 100} };
    m_descriptorPool = dev.createDescriptorPool({ vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 2000, (uint32_t)pSizes.size(), pSizes.data() });
    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_globalDescriptorLayout);
    m_globalDescriptorSets = dev.allocateDescriptorSets({ m_descriptorPool, MAX_FRAMES_IN_FLIGHT, layouts.data() });
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vk::DescriptorBufferInfo camInfo(m_cameraUbos[i]->getHandle(), 0, sizeof(GlobalUBO));
        vk::DescriptorBufferInfo instInfo(m_instanceBuffers[i]->getHandle(), 0, sizeof(glm::mat4) * MAX_INSTANCES);
        std::vector<vk::WriteDescriptorSet> writes = { {m_globalDescriptorSets[i], 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &camInfo}, {m_globalDescriptorSets[i], 1, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &instInfo} };
        dev.updateDescriptorSets(writes, {});
    }
}

void Renderer::createPipelines(const EngineConfig& config) {
    auto dev = m_context.getDevice();
    m_layouts[MaterialType::PBR] = PBRMaterial::CreateLayout(dev);
    m_layouts[MaterialType::Unlit] = UnlitMaterial::CreateLayout(dev);
    m_layouts[MaterialType::Toon] = ToonMaterial::CreateLayout(dev);
    m_layouts[MaterialType::Skybox] = SkyboxMaterial::CreateLayout(dev);
    m_layouts[MaterialType::SkySphere] = SkySphereMaterial::CreateLayout(dev);

    m_shaders["pbr.vert"] = CreateScope<Shader>(m_context, "assets/shaders/pbr.vert.spv");
    m_shaders["pbr.frag"] = CreateScope<Shader>(m_context, "assets/shaders/pbr.frag.spv");
    m_shaders["unlit.vert"] = CreateScope<Shader>(m_context, "assets/shaders/unlit.vert.spv");
    m_shaders["unlit.frag"] = CreateScope<Shader>(m_context, "assets/shaders/unlit.frag.spv");
    m_shaders["toon.vert"] = CreateScope<Shader>(m_context, "assets/shaders/toon.vert.spv");
    m_shaders["toon.frag"] = CreateScope<Shader>(m_context, "assets/shaders/toon.frag.spv");
    m_shaders["skybox.vert"] = CreateScope<Shader>(m_context, "assets/shaders/skybox.vert.spv");
    m_shaders["skybox.frag"] = CreateScope<Shader>(m_context, "assets/shaders/skybox.frag.spv");
    m_shaders["skysphere.vert"] = CreateScope<Shader>(m_context, "assets/shaders/skysphere.vert.spv");
    m_shaders["skysphere.frag"] = CreateScope<Shader>(m_context, "assets/shaders/skysphere.frag.spv");
    m_shaders["fullscreen.vert"] = CreateScope<Shader>(m_context, "assets/shaders/fullscreen.vert.spv");
    m_shaders["copy.frag"] = CreateScope<Shader>(m_context, "assets/shaders/copy.frag.spv");

    vk::Format colorFmt = m_config.graphics.enableOffscreenRendering ? m_renderTarget->getColorFormat() : m_swapChain->getImageFormat();
    vk::Format depthFmt = m_config.graphics.enableOffscreenRendering ? m_renderTarget->getDepthFormat() : m_swapChain->getDepthFormat();

    auto createP = [&](MaterialType t, const std::string& v, const std::string& f, const EngineConfig& cfg, bool dWrite, vk::CompareOp op, const std::vector<uint32_t>& attr = {}) {
        std::vector<vk::DescriptorSetLayout> ls = { m_globalDescriptorLayout, m_layouts[t] };
        return CreateScope<GraphicsPipeline>(m_context, colorFmt, depthFmt, *m_shaders[v], *m_shaders[f], cfg, ls, std::vector<vk::PushConstantRange>{}, true, dWrite, op, attr);
    };

    m_pipelines[MaterialType::PBR] = createP(MaterialType::PBR, "pbr.vert", "pbr.frag", config, true, vk::CompareOp::eLess);
    EngineConfig envCfg = config; envCfg.rasterizer.setCullMode("None");
    m_pipelines[MaterialType::Unlit] = createP(MaterialType::Unlit, "unlit.vert", "unlit.frag", envCfg, true, vk::CompareOp::eLess);
    m_pipelines[MaterialType::Toon] = createP(MaterialType::Toon, "toon.vert", "toon.frag", config, true, vk::CompareOp::eLess);
    std::vector<uint32_t> envAttr = { 0, 1, 2, 3, 4 };
    m_pipelines[MaterialType::Skybox] = createP(MaterialType::Skybox, "skybox.vert", "skybox.frag", envCfg, false, vk::CompareOp::eAlways, envAttr);
    m_pipelines[MaterialType::SkySphere] = createP(MaterialType::SkySphere, "skysphere.vert", "skysphere.frag", envCfg, false, vk::CompareOp::eAlways, envAttr);
}

void Renderer::createCopyPipeline() {
    if (!m_config.graphics.enableOffscreenRendering) return;
    vk::DescriptorSetLayoutBinding binding{0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment};
    m_copyLayout = m_context.getDevice().createDescriptorSetLayout({{}, 1, &binding});
    EngineConfig copyConfig = m_config; copyConfig.rasterizer.setCullMode("None");
    m_copyPipeline = CreateScope<GraphicsPipeline>(m_context, m_swapChain->getImageFormat(), vk::Format::eUndefined, *m_shaders["fullscreen.vert"], *m_shaders["copy.frag"], copyConfig, std::vector<vk::DescriptorSetLayout>{m_copyLayout}, std::vector<vk::PushConstantRange>{}, false, false); 
    vk::DescriptorSetAllocateInfo allocInfo(m_descriptorPool, 1, &m_copyLayout);
    m_copyDescriptorSet = m_context.getDevice().allocateDescriptorSets(allocInfo)[0];
    vk::DescriptorImageInfo imgInfo(m_renderTarget->getSampler(), m_renderTarget->getColorImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
    vk::WriteDescriptorSet write(m_copyDescriptorSet, 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imgInfo, nullptr, nullptr);
    m_context.getDevice().updateDescriptorSets(1, &write, 0, nullptr);
}

Ref<Material> Renderer::getMaterialForTexture(Ref<Texture> texture) {
    if (!texture) return nullptr;
    std::string key = std::string(texture->getPath());
    if (key.empty()) key = "gen_" + std::to_string((uintptr_t)texture.get());
    if (m_defaultMaterials.contains(key)) return m_defaultMaterials[key];
    auto material = CreateRef<PBRMaterial>(m_context); material->setAlbedoMap(texture);
    m_defaultMaterials[key] = material; return material;
}

void Renderer::renderUI(const std::function<void(vk::CommandBuffer)>& func) {
    auto& cb = m_commandBuffers[m_currentFrame];
    uint32_t imageIndex = m_swapChain->getCurrentImageIndex();
    vk::ImageView view = m_swapChain->getImageViews()[imageIndex];
    vk::Extent2D extent = m_swapChain->getExtent();
    vk::RenderingAttachmentInfo colorAttr(view, vk::ImageLayout::eColorAttachmentOptimal, vk::ResolveModeFlagBits::eNone, {}, vk::ImageLayout::eUndefined, vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore, vk::ClearValue());
    cb.beginRendering({ {}, {{0, 0}, extent}, 1, 0, 1, &colorAttr, nullptr });
    func(cb);
    cb.endRendering();
}

void Renderer::onResize(int width, int height) {
    if (width == 0 || height == 0) return;
    m_swapChain->recreate(width, height);
    m_imagesInUseFences.resize(m_swapChain->getImageCount(), nullptr);
    if (m_renderFinishedSemaphores.size() != m_swapChain->getImageCount()) {
        auto dev = m_context.getDevice();
        for (auto s : m_renderFinishedSemaphores) if (s) dev.destroySemaphore(s);
        m_renderFinishedSemaphores.clear(); m_renderFinishedSemaphores.resize(m_swapChain->getImageCount());
        for (size_t i = 0; i < m_renderFinishedSemaphores.size(); i++) m_renderFinishedSemaphores[i] = dev.createSemaphore({});
    }
    if (m_renderTarget) {
        uint32_t w = static_cast<uint32_t>(width * m_config.graphics.renderScale);
        uint32_t h = static_cast<uint32_t>(height * m_config.graphics.renderScale);
        w = std::max(1u, w); h = std::max(1u, h);
        m_renderTarget->resize(w, h);
        vk::DescriptorImageInfo imgInfo(m_renderTarget->getSampler(), m_renderTarget->getColorImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
        vk::WriteDescriptorSet write(m_copyDescriptorSet, 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imgInfo, nullptr, nullptr);
        m_context.getDevice().updateDescriptorSets(1, &write, 0, nullptr);
    }
}

void Renderer::render(Scene& scene) {
    if (m_window.GetWidth() == 0 || m_window.GetHeight() == 0) return;
    auto dev = m_context.getDevice();
    (void)dev.waitForFences(1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

    Camera* activeCamera = nullptr;
    auto camView = scene.getRegistry().view<CameraComponent>();
    for (auto entity : camView) { if (camView.get<CameraComponent>(entity).active) { activeCamera = camView.get<CameraComponent>(entity).camera.get(); break; } }
    if (!activeCamera) return;

    GlobalUBO uboData; uboData.view = activeCamera->getViewMatrix(); uboData.proj = activeCamera->getProjectionMatrix(); uboData.camPos = glm::vec4(glm::vec3(glm::inverse(uboData.view)[3]), 0.0f);
    if (m_config.graphics.enableFrustumCulling) m_frustum.update(uboData.proj * uboData.view);
    
    int numLights = 0;
    auto lightView = scene.getRegistry().view<LightComponent>();
    for (auto entity : lightView) {
        if (numLights >= 10) break;
        auto& light = lightView.get<LightComponent>(entity);
        ShaderLight& sl = uboData.lights[numLights];
        sl.color = glm::vec4(light.color, light.intensity); sl.params = glm::vec4(light.range, 0.0f, 0.0f, 0.0f);
        if (light.type == LightType::Directional) { sl.position.w = 0.0f; if (scene.getRegistry().all_of<TransformComponent>(entity)) sl.direction = glm::vec4(scene.getRegistry().get<TransformComponent>(entity).getForward(), 0.0f); else sl.direction = glm::vec4(0.0f, -1.0f, 0.0f, 0.0f); }
        else if (light.type == LightType::Point) { sl.position.w = 1.0f; if (scene.getRegistry().all_of<TransformComponent>(entity)) sl.position = glm::vec4(scene.getRegistry().get<TransformComponent>(entity).translation, 1.0f); }
        numLights++;
    }
    uboData.globalParams = glm::vec4((float)numLights, 0.0f, 0.0f, 0.0f);
    m_cameraUbos[m_currentFrame]->update(&uboData, sizeof(uboData));

    uint32_t imageIndex;
    try { imageIndex = m_swapChain->acquireNextImage(m_imageAvailableSemaphores[m_currentFrame]); } 
    catch (...) { onResize(m_window.GetWidth(), m_window.GetHeight()); return; }

    if (m_imagesInUseFences[imageIndex]) (void)dev.waitForFences(1, &m_imagesInUseFences[imageIndex], VK_TRUE, UINT64_MAX);
    m_imagesInUseFences[imageIndex] = m_inFlightFences[m_currentFrame];
    (void)dev.resetFences(1, &m_inFlightFences[m_currentFrame]);

    auto& cb = m_commandBuffers[m_currentFrame];
    cb.reset({}); cb.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

    if (m_config.graphics.enableOffscreenRendering) {
        vk::Image rtImage = m_renderTarget->getColorImage();
        vk::ImageMemoryBarrier rtBarrier({}, vk::AccessFlagBits::eColorAttachmentWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, rtImage, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
        cb.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, nullptr, nullptr, rtBarrier);
        vk::Image dImage = m_renderTarget->getDepthImage();
        vk::ImageMemoryBarrier dBarrier({}, vk::AccessFlagBits::eDepthStencilAttachmentWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, dImage, { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 });
        cb.pipelineBarrier(vk::PipelineStageFlagBits::eEarlyFragmentTests, vk::PipelineStageFlagBits::eEarlyFragmentTests, {}, nullptr, nullptr, dBarrier);
        drawScene(cb, scene, m_renderTarget->getColorImageView(), m_renderTarget->getDepthImageView(), m_renderTarget->getExtent());
        compositeToSwapchain(cb, imageIndex);

        // Transition pour que ImGui puisse lire la texture du RenderTarget
        vk::ImageMemoryBarrier rtReadBarrier(vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eShaderRead, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, rtImage, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
        cb.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eFragmentShader, {}, nullptr, nullptr, rtReadBarrier);
    } else {
        vk::Image swImage = m_swapChain->getImage(imageIndex);
        vk::ImageMemoryBarrier swBarrier({}, vk::AccessFlagBits::eColorAttachmentWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, swImage, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
        cb.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, nullptr, nullptr, swBarrier);
        vk::Image dImage = m_swapChain->getDepthImage();
        vk::ImageMemoryBarrier dBarrier({}, vk::AccessFlagBits::eDepthStencilAttachmentWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, dImage, { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 });
        cb.pipelineBarrier(vk::PipelineStageFlagBits::eEarlyFragmentTests, vk::PipelineStageFlagBits::eEarlyFragmentTests, {}, nullptr, nullptr, dBarrier);
        drawScene(cb, scene, m_swapChain->getImageViews()[imageIndex], m_swapChain->getDepthImageView(), m_swapChain->getExtent());
    }
    
    // Le CB reste ouvert pour renderUI si besoin
}

void Renderer::submitAndPresent() {
    auto& cb = m_commandBuffers[m_currentFrame];
    uint32_t imageIndex = m_swapChain->getCurrentImageIndex();
    vk::Image swImage = m_swapChain->getImage(imageIndex);

    // Transition finale pour présentation
    vk::ImageMemoryBarrier presentBarrier(vk::AccessFlagBits::eColorAttachmentWrite, {}, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, swImage, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
    cb.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eBottomOfPipe, {}, nullptr, nullptr, presentBarrier);

    cb.end();

    vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
    vk::SubmitInfo submitInfo(1, &m_imageAvailableSemaphores[m_currentFrame], waitStages, 1, &cb, 1, &m_renderFinishedSemaphores[imageIndex]);
    
    try {
        m_context.getGraphicsQueue().submit(submitInfo, m_inFlightFences[m_currentFrame]);
        m_swapChain->present(m_renderFinishedSemaphores[imageIndex], imageIndex);
    } catch (...) { onResize(m_window.GetWidth(), m_window.GetHeight()); }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::drawScene(vk::CommandBuffer cb, Scene& scene, vk::ImageView colorView, vk::ImageView depthView, vk::Extent2D extent) {
    vk::RenderingAttachmentInfo colorAttr(colorView, vk::ImageLayout::eColorAttachmentOptimal, vk::ResolveModeFlagBits::eNone, {}, vk::ImageLayout::eUndefined, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::ClearValue(vk::ClearColorValue(std::array<float, 4>{0.1f, 0.1f, 0.1f, 1.0f})));
    vk::RenderingAttachmentInfo depthAttr(depthView, vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::ResolveModeFlagBits::eNone, {}, vk::ImageLayout::eUndefined, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::ClearValue(vk::ClearDepthStencilValue(1.0f, 0)));
    cb.beginRendering({ {}, {{0, 0}, extent}, 1, 0, 1, &colorAttr, &depthAttr });
    cb.setViewport(0, vk::Viewport(0, 0, (float)extent.width, (float)extent.height, 0, 1));
    cb.setScissor(0, vk::Rect2D({0, 0}, extent));
    renderSkybox(cb, scene);
    m_renderCommands.clear();
    auto meshView = scene.getRegistry().view<MeshComponent, TransformComponent>();
    for (auto entity : meshView) {
        auto& meshComp = meshView.get<MeshComponent>(entity); 
        if (!meshComp.mesh || !meshComp.visible) continue;
        glm::mat4 transform = meshView.get<TransformComponent>(entity).getTransform();
        if (m_config.graphics.enableFrustumCulling) { AABB worldBox = meshComp.mesh->getBounds().transform(transform); if (!m_frustum.intersects(worldBox)) continue; }
        Material* mat = meshComp.mesh->getMaterial().get(); if (!mat) mat = m_fallbackMaterial.get();
        m_renderCommands.push_back({ mat->getType(), mat, meshComp.mesh.get(), transform });
    }
    auto modelView = scene.getRegistry().view<ModelComponent, TransformComponent>();
    for (auto entity : modelView) {
        auto& modelComp = modelView.get<ModelComponent>(entity); 
        if (!modelComp.model || !modelComp.visible) continue;
        glm::mat4 transform = modelView.get<TransformComponent>(entity).getTransform();
        if (m_config.graphics.enableFrustumCulling) { AABB worldBox = modelComp.model->getBounds().transform(transform); if (!m_frustum.intersects(worldBox)) continue; }
        for (const auto& mesh : modelComp.model->getMeshes()) {
            Material* mat = mesh->getMaterial().get(); if (!mat) { Ref<Texture> tex = mesh->getTexture(); mat = tex ? getMaterialForTexture(tex).get() : nullptr; }
            if (!mat) mat = m_fallbackMaterial.get();
            m_renderCommands.push_back({ mat->getType(), mat, mesh.get(), transform });
        }
    }
    std::sort(m_renderCommands.begin(), m_renderCommands.end());
    GraphicsPipeline* lastPipeline = nullptr; Material* lastMaterial = nullptr; Mesh* lastMesh = nullptr;
    m_instanceTransforms.clear();
    uint32_t currentInstanceOffset = 0;
    auto flushInstances = [&]() {
        if (m_instanceTransforms.empty()) return;
        if (currentInstanceOffset + m_instanceTransforms.size() > MAX_INSTANCES) { m_instanceTransforms.clear(); return; }
        void* data = static_cast<char*>(m_instanceBuffers[m_currentFrame]->getMappedData()) + (currentInstanceOffset * sizeof(glm::mat4));
        memcpy(data, m_instanceTransforms.data(), m_instanceTransforms.size() * sizeof(glm::mat4));
        lastMesh->draw(cb, (uint32_t)m_instanceTransforms.size(), currentInstanceOffset);
        currentInstanceOffset += (uint32_t)m_instanceTransforms.size(); m_instanceTransforms.clear();
    };
    for (const auto& cmd : m_renderCommands) {
        if (m_instanceTransforms.size() >= MAX_INSTANCES || (lastMesh && cmd.mesh != lastMesh) || (lastMaterial && cmd.material != lastMaterial)) flushInstances();
        if (cmd.type != MaterialType::Skybox && cmd.type != MaterialType::SkySphere) {
            auto& pipeline = m_pipelines[cmd.type]; bool pipelineChanged = false;
            if (pipeline.get() != lastPipeline) { pipeline->bind(cb); lastPipeline = pipeline.get(); cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->getLayout(), 0, 1, &m_globalDescriptorSets[m_currentFrame], 0, nullptr); pipelineChanged = true; }
            if (cmd.material != lastMaterial || pipelineChanged) { vk::DescriptorSet ds = cmd.material->getDescriptorSet(m_descriptorPool, m_layouts[cmd.type]); cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->getLayout(), 1, 1, &ds, 0, nullptr); lastMaterial = cmd.material; }
            lastMesh = cmd.mesh; m_instanceTransforms.push_back(cmd.transform);
        }
    }
    flushInstances(); cb.endRendering();
}

void Renderer::renderSkybox(vk::CommandBuffer cb, Scene& scene) {
    auto skySphereView = scene.getRegistry().view<SkySphereComponent>();
    if (!skySphereView.empty()) {
        auto entity = skySphereView.front(); auto& sky = skySphereView.get<SkySphereComponent>(entity);
        if (sky.texture) {
            m_internalSkySphereMat->setTexture(sky.texture); auto& pipeline = m_pipelines[MaterialType::SkySphere]; pipeline->bind(cb);
            cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->getLayout(), 0, 1, &m_globalDescriptorSets[m_currentFrame], 0, nullptr);
            vk::DescriptorSet ds = m_internalSkySphereMat->getDescriptorSet(m_descriptorPool, m_layouts[MaterialType::SkySphere]);
            cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->getLayout(), 1, 1, &ds, 0, nullptr);
            m_skyboxCube->draw(cb); return; 
        }
    }
    if (scene.getSkybox()) {
        m_internalSkyboxMat->setCubemap(scene.getSkybox()); auto& pipeline = m_pipelines[MaterialType::Skybox]; pipeline->bind(cb);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->getLayout(), 0, 1, &m_globalDescriptorSets[m_currentFrame], 0, nullptr);
        vk::DescriptorSet ds = m_internalSkyboxMat->getDescriptorSet(m_descriptorPool, m_layouts[MaterialType::Skybox]);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->getLayout(), 1, 1, &ds, 0, nullptr);
        m_skyboxCube->draw(cb);
    }
}

void Renderer::compositeToSwapchain(vk::CommandBuffer cb, uint32_t imageIndex) {
    vk::Image swapImage = m_swapChain->getImage(imageIndex);
    vk::ImageMemoryBarrier barrier({}, vk::AccessFlagBits::eColorAttachmentWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, swapImage, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
    cb.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, nullptr, nullptr, barrier);
    vk::Image rtImage = m_renderTarget->getColorImage();
    vk::ImageMemoryBarrier rtBarrier(vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eShaderRead, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, rtImage, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
    cb.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eFragmentShader, {}, nullptr, nullptr, rtBarrier);
    vk::RenderingAttachmentInfo colorAttr(m_swapChain->getImageViews()[imageIndex], vk::ImageLayout::eColorAttachmentOptimal, vk::ResolveModeFlagBits::eNone, {}, vk::ImageLayout::eUndefined, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::ClearValue(vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f})));
    cb.beginRendering({ {}, {{0, 0}, m_swapChain->getExtent()}, 1, 0, 1, &colorAttr, nullptr });
    cb.setViewport(0, vk::Viewport(0, 0, (float)m_swapChain->getExtent().width, (float)m_swapChain->getExtent().height, 0, 1));
    cb.setScissor(0, vk::Rect2D({0, 0}, m_swapChain->getExtent()));
    m_copyPipeline->bind(cb);
    cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_copyPipeline->getLayout(), 0, 1, &m_copyDescriptorSet, 0, nullptr);
    cb.draw(3, 1, 0, 0);
    cb.endRendering();
    vk::ImageMemoryBarrier resetBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eColorAttachmentWrite, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eColorAttachmentOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, rtImage, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
    cb.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, nullptr, nullptr, resetBarrier);
}

} // namespace bb3d