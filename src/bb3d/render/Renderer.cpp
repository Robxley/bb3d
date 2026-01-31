#include "bb3d/render/Renderer.hpp"
#include "bb3d/core/Window.hpp"
#include "bb3d/core/Log.hpp"
#include "bb3d/render/UniformBuffer.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/render/MeshGenerator.hpp"
#include <array>
#include <SDL3/SDL.h>

namespace bb3d {

Renderer::Renderer(VulkanContext& context, Window& window, const EngineConfig& config)
    : m_context(context), m_window(window) {
    m_swapChain = CreateScope<SwapChain>(context, config.window.width, config.window.height);
    createSyncObjects();
    createGlobalDescriptors();
    createPipelines(config);
    m_skyboxCube = MeshGenerator::createCube(m_context, 1.0f);
    
    // Matériaux internes pour l'environnement (pour éviter les fuites statiques)
    m_internalSkyboxMat = CreateRef<SkyboxMaterial>(m_context);
    m_internalSkySphereMat = CreateRef<SkySphereMaterial>(m_context);
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
        
        m_internalSkyboxMat.reset();
        m_internalSkySphereMat.reset();
        m_skyboxCube.reset();
        m_defaultMaterials.clear();
        m_pipelines.clear();
        m_shaders.clear();
        
        if (m_descriptorPool) dev.destroyDescriptorPool(m_descriptorPool);
        if (m_globalDescriptorLayout) dev.destroyDescriptorSetLayout(m_globalDescriptorLayout);
        for (auto& [type, layout] : m_layouts) { if(layout) dev.destroyDescriptorSetLayout(layout); }
        if (m_commandPool) dev.destroyCommandPool(m_commandPool);
    }
}

void Renderer::createSyncObjects() {
    auto dev = m_context.getDevice();
    m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        m_imageAvailableSemaphores[i] = dev.createSemaphore({});
        m_renderFinishedSemaphores[i] = dev.createSemaphore({});
        m_inFlightFences[i] = dev.createFence({ vk::FenceCreateFlagBits::eSignaled });
    }
    m_commandPool = dev.createCommandPool({ vk::CommandPoolCreateFlagBits::eResetCommandBuffer, m_context.getGraphicsQueueFamily() });
    m_commandBuffers = dev.allocateCommandBuffers({ m_commandPool, vk::CommandBufferLevel::ePrimary, MAX_FRAMES_IN_FLIGHT });
}

void Renderer::createGlobalDescriptors() {
    auto dev = m_context.getDevice();
    m_cameraUbo = CreateScope<UniformBuffer>(m_context, sizeof(GlobalUBO));
    vk::DescriptorSetLayoutBinding cameraBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
    m_globalDescriptorLayout = dev.createDescriptorSetLayout({ {}, 1, &cameraBinding });

    std::vector<vk::DescriptorPoolSize> pSizes;
    pSizes.push_back({vk::DescriptorType::eUniformBuffer, 500}); // Plus large pour les matériaux
    pSizes.push_back({vk::DescriptorType::eCombinedImageSampler, 1000});
    m_descriptorPool = dev.createDescriptorPool({ vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1000, (uint32_t)pSizes.size(), pSizes.data() });

    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_globalDescriptorLayout);
    m_globalDescriptorSets = dev.allocateDescriptorSets({ m_descriptorPool, MAX_FRAMES_IN_FLIGHT, layouts.data() });

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vk::DescriptorBufferInfo bufferInfo(m_cameraUbo->getHandle(), 0, sizeof(GlobalUBO));
        vk::WriteDescriptorSet write(m_globalDescriptorSets[i], 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &bufferInfo);
        dev.updateDescriptorSets(1, &write, 0, nullptr);
    }
}

void Renderer::createPipelines(const EngineConfig& config) {
    auto dev = m_context.getDevice();
    m_layouts[MaterialType::PBR] = PBRMaterial(m_context).getDescriptorSetLayout(dev);
    m_layouts[MaterialType::Unlit] = UnlitMaterial(m_context).getDescriptorSetLayout(dev);
    m_layouts[MaterialType::Toon] = ToonMaterial(m_context).getDescriptorSetLayout(dev);
    m_layouts[MaterialType::Skybox] = SkyboxMaterial(m_context).getDescriptorSetLayout(dev);
    m_layouts[MaterialType::SkySphere] = SkySphereMaterial(m_context).getDescriptorSetLayout(dev);

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

    vk::PushConstantRange pcr(vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4));
    
    auto createP = [&](MaterialType t, const std::string& v, const std::string& f, const EngineConfig& cfg, bool dWrite, vk::CompareOp op, bool useP) {
        std::vector<vk::DescriptorSetLayout> ls = { m_globalDescriptorLayout, m_layouts[t] };
        std::vector<vk::PushConstantRange> ps; if(useP) ps.push_back(pcr);
        return CreateScope<GraphicsPipeline>(m_context, *m_swapChain, *m_shaders[v], *m_shaders[f], cfg, ls, ps, true, dWrite, op);
    };

    m_pipelines[MaterialType::PBR] = createP(MaterialType::PBR, "pbr.vert", "pbr.frag", config, true, vk::CompareOp::eLess, true);
    EngineConfig envCfg = config; envCfg.rasterizer.setCullMode("None");
    m_pipelines[MaterialType::Unlit] = createP(MaterialType::Unlit, "unlit.vert", "unlit.frag", envCfg, true, vk::CompareOp::eLess, true);
    m_pipelines[MaterialType::Toon] = createP(MaterialType::Toon, "toon.vert", "toon.frag", config, true, vk::CompareOp::eLess, true);
    m_pipelines[MaterialType::Skybox] = createP(MaterialType::Skybox, "skybox.vert", "skybox.frag", envCfg, false, vk::CompareOp::eAlways, false);
    m_pipelines[MaterialType::SkySphere] = createP(MaterialType::SkySphere, "skysphere.vert", "skysphere.frag", envCfg, false, vk::CompareOp::eAlways, false);
}

Ref<Material> Renderer::getMaterialForTexture(Ref<Texture> texture) {
    if (!texture) return nullptr;
    if (m_defaultMaterials.contains(texture.get())) return m_defaultMaterials[texture.get()];
    auto material = CreateRef<PBRMaterial>(m_context); material->setAlbedoMap(texture);
    m_defaultMaterials[texture.get()] = material;
    return material;
}

void Renderer::render(Scene& scene) {
    auto dev = m_context.getDevice();
    (void)dev.waitForFences(1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);
    Camera* activeCamera = nullptr;
    auto camView = scene.getRegistry().view<CameraComponent>();
    for (auto entity : camView) { if (camView.get<CameraComponent>(entity).active) { activeCamera = camView.get<CameraComponent>(entity).camera.get(); break; } }
    if (!activeCamera) return;

    GlobalUBO uboData; uboData.view = activeCamera->getViewMatrix(); uboData.proj = activeCamera->getProjectionMatrix(); uboData.camPos = glm::vec3(glm::inverse(uboData.view)[3]);
    m_cameraUbo->update(&uboData, sizeof(uboData));

    uint32_t imageIndex;
    try { imageIndex = m_swapChain->acquireNextImage(m_imageAvailableSemaphores[m_currentFrame]); } catch (...) { return; }
    (void)dev.resetFences(1, &m_inFlightFences[m_currentFrame]);

    auto& cb = m_commandBuffers[m_currentFrame];
    cb.reset({}); cb.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

    vk::Image colorImage = m_swapChain->getImage(imageIndex);
    vk::Image depthImage = m_swapChain->getDepthImage();
    std::vector<vk::ImageMemoryBarrier> barriers = {
        { {}, vk::AccessFlagBits::eColorAttachmentWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, colorImage, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 } },
        { {}, vk::AccessFlagBits::eDepthStencilAttachmentWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, depthImage, { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 } }
    };
    cb.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests, {}, nullptr, nullptr, barriers);

    vk::RenderingAttachmentInfo colorAttr(m_swapChain->getImageViews()[imageIndex], vk::ImageLayout::eColorAttachmentOptimal, vk::ResolveModeFlagBits::eNone, {}, vk::ImageLayout::eUndefined, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::ClearValue(vk::ClearColorValue(std::array<float, 4>{0.1f, 0.1f, 0.1f, 1.0f})));
    vk::RenderingAttachmentInfo depthAttr(m_swapChain->getDepthImageView(), vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::ResolveModeFlagBits::eNone, {}, vk::ImageLayout::eUndefined, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::ClearValue(vk::ClearDepthStencilValue(1.0f, 0)));
    cb.beginRendering({ {}, {{0, 0}, m_swapChain->getExtent()}, 1, 0, 1, &colorAttr, &depthAttr });
    cb.setViewport(0, vk::Viewport(0, 0, (float)m_swapChain->getExtent().width, (float)m_swapChain->getExtent().height, 0, 1));
    cb.setScissor(0, vk::Rect2D({0, 0}, m_swapChain->getExtent()));
    
    renderSkybox(cb, scene);

    auto meshView = scene.getRegistry().view<MeshComponent, TransformComponent>();
    for (auto entity : meshView) {
        auto& meshComp = meshView.get<MeshComponent>(entity);
        auto& transform = meshView.get<TransformComponent>(entity);
        if (!meshComp.mesh) continue;
        Ref<Material> mat = meshComp.mesh->getMaterial();
        if (!mat) { Ref<Texture> tex = meshComp.mesh->getTexture(); mat = tex ? getMaterialForTexture(tex) : nullptr; }
        if (!mat) { static Ref<Material> def = nullptr; if(!def) def = CreateRef<PBRMaterial>(m_context); mat = def; }
        m_pipelines[mat->getType()]->bind(cb);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelines[mat->getType()]->getLayout(), 0, 1, &m_globalDescriptorSets[m_currentFrame], 0, nullptr);
        vk::DescriptorSet matSet = mat->getDescriptorSet(m_descriptorPool);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelines[mat->getType()]->getLayout(), 1, 1, &matSet, 0, nullptr);
        glm::mat4 modelMat = transform.getTransform();
        cb.pushConstants(m_pipelines[mat->getType()]->getLayout(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4), &modelMat);
        meshComp.mesh->draw(cb);
    }

    auto modelView = scene.getRegistry().view<ModelComponent, TransformComponent>();
    for (auto entity : modelView) {
        auto& modelComp = modelView.get<ModelComponent>(entity);
        auto& transform = modelView.get<TransformComponent>(entity);
        if (modelComp.model) {
            glm::mat4 modelMat = transform.getTransform();
            for (const auto& mesh : modelComp.model->getMeshes()) {
                Ref<Material> mat = mesh->getMaterial();
                if (!mat) { Ref<Texture> tex = mesh->getTexture(); mat = tex ? getMaterialForTexture(tex) : nullptr; }
                if (!mat) { static Ref<Material> def = nullptr; if(!def) def = CreateRef<PBRMaterial>(m_context); mat = def; }
                m_pipelines[mat->getType()]->bind(cb);
                cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelines[mat->getType()]->getLayout(), 0, 1, &m_globalDescriptorSets[m_currentFrame], 0, nullptr);
                vk::DescriptorSet matSet = mat->getDescriptorSet(m_descriptorPool);
                cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelines[mat->getType()]->getLayout(), 1, 1, &matSet, 0, nullptr);
                cb.pushConstants(m_pipelines[mat->getType()]->getLayout(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4), &modelMat);
                mesh->draw(cb);
            }
        }
    }

    cb.endRendering();
    cb.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eBottomOfPipe, {}, nullptr, nullptr, vk::ImageMemoryBarrier(vk::AccessFlagBits::eColorAttachmentWrite, {}, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, colorImage, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }));
    cb.end();

    vk::PipelineStageFlags waitStages = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    m_context.getGraphicsQueue().submit(vk::SubmitInfo(1, &m_imageAvailableSemaphores[m_currentFrame], &waitStages, 1, &cb, 1, &m_renderFinishedSemaphores[m_currentFrame]), m_inFlightFences[m_currentFrame]);
    try { m_swapChain->present(m_renderFinishedSemaphores[m_currentFrame], imageIndex); } catch (...) {}
    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::renderSkybox(vk::CommandBuffer cb, Scene& scene) {
    Ref<Texture> skyboxTex = scene.getSkybox();
    if (skyboxTex && skyboxTex->isCubemap()) {
        auto& pipeline = m_pipelines[MaterialType::Skybox]; pipeline->bind(cb);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->getLayout(), 0, 1, &m_globalDescriptorSets[m_currentFrame], 0, nullptr);
        m_internalSkyboxMat->setCubemap(skyboxTex);
        vk::DescriptorSet matSet = m_internalSkyboxMat->getDescriptorSet(m_descriptorPool);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->getLayout(), 1, 1, &matSet, 0, nullptr);
        m_skyboxCube->draw(cb);
    }
    auto skySphereView = scene.getRegistry().view<SkySphereComponent>();
    for (auto entity : skySphereView) {
        auto& comp = skySphereView.get<SkySphereComponent>(entity);
        if (comp.texture) {
            auto& pipeline = m_pipelines[MaterialType::SkySphere]; pipeline->bind(cb);
            cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->getLayout(), 0, 1, &m_globalDescriptorSets[m_currentFrame], 0, nullptr);
            m_internalSkySphereMat->setTexture(comp.texture);
            vk::DescriptorSet matSet = m_internalSkySphereMat->getDescriptorSet(m_descriptorPool);
            cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->getLayout(), 1, 1, &matSet, 0, nullptr);
            m_skyboxCube->draw(cb);
        }
    }
}

void Renderer::onResize(int width, int height) { m_swapChain->recreate(width, height); }

} // namespace bb3d