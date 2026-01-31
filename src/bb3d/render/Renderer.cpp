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

struct RenderCommand {
    MaterialType type;
    Material* material;
    Mesh* mesh;
    glm::mat4 transform;

    bool operator<(const RenderCommand& other) const {
        if (type != other.type) return type < other.type;
        if (material != other.material) return material < other.material;
        return mesh < other.mesh;
    }
};

Renderer::Renderer(VulkanContext& context, Window& window, const EngineConfig& config)
    : m_context(context), m_window(window) {
    m_swapChain = CreateScope<SwapChain>(context, config.window.width, config.window.height);
    createSyncObjects();
    createGlobalDescriptors();
    createPipelines(config);
    m_skyboxCube = MeshGenerator::createCube(m_context, 1.0f);
    m_internalSkyboxMat = CreateRef<SkyboxMaterial>(m_context);
    m_internalSkySphereMat = CreateRef<SkySphereMaterial>(m_context);
    m_fallbackMaterial = CreateRef<PBRMaterial>(m_context);
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
        m_fallbackMaterial.reset();
        m_skyboxCube.reset();
        m_cameraUbos.clear();
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
    m_cameraUbos.resize(MAX_FRAMES_IN_FLIGHT);
    for(uint32_t i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) m_cameraUbos[i] = CreateScope<UniformBuffer>(m_context, sizeof(GlobalUBO));

    vk::DescriptorSetLayoutBinding binding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
    m_globalDescriptorLayout = dev.createDescriptorSetLayout({ {}, 1, &binding });

    std::vector<vk::DescriptorPoolSize> pSizes = { {vk::DescriptorType::eUniformBuffer, 500}, {vk::DescriptorType::eCombinedImageSampler, 1000} };
    m_descriptorPool = dev.createDescriptorPool({ vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1000, (uint32_t)pSizes.size(), pSizes.data() });

    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_globalDescriptorLayout);
    m_globalDescriptorSets = dev.allocateDescriptorSets({ m_descriptorPool, MAX_FRAMES_IN_FLIGHT, layouts.data() });

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vk::DescriptorBufferInfo bufferInfo(m_cameraUbos[i]->getHandle(), 0, sizeof(GlobalUBO));
        vk::WriteDescriptorSet write(m_globalDescriptorSets[i], 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &bufferInfo);
        dev.updateDescriptorSets(1, &write, 0, nullptr);
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

    vk::PushConstantRange pcr(vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4));
    auto createP = [&](MaterialType t, const std::string& v, const std::string& f, const EngineConfig& cfg, bool dWrite, vk::CompareOp op, bool useP, const std::vector<uint32_t>& attr = {}) {
        std::vector<vk::DescriptorSetLayout> ls = { m_globalDescriptorLayout, m_layouts[t] };
        std::vector<vk::PushConstantRange> ps; if(useP) ps.push_back(pcr);
        return CreateScope<GraphicsPipeline>(m_context, *m_swapChain, *m_shaders[v], *m_shaders[f], cfg, ls, ps, true, dWrite, op, attr);
    };

    m_pipelines[MaterialType::PBR] = createP(MaterialType::PBR, "pbr.vert", "pbr.frag", config, true, vk::CompareOp::eLess, true);
    EngineConfig envCfg = config; envCfg.rasterizer.setCullMode("None");
    m_pipelines[MaterialType::Unlit] = createP(MaterialType::Unlit, "unlit.vert", "unlit.frag", envCfg, true, vk::CompareOp::eLess, true);
    m_pipelines[MaterialType::Toon] = createP(MaterialType::Toon, "toon.vert", "toon.frag", config, true, vk::CompareOp::eLess, true);
    std::vector<uint32_t> envAttr = { 0, 1, 2, 3, 4 };
    m_pipelines[MaterialType::Skybox] = createP(MaterialType::Skybox, "skybox.vert", "skybox.frag", envCfg, false, vk::CompareOp::eAlways, false, envAttr);
    m_pipelines[MaterialType::SkySphere] = createP(MaterialType::SkySphere, "skysphere.vert", "skysphere.frag", envCfg, false, vk::CompareOp::eAlways, false, envAttr);
}

Ref<Material> Renderer::getMaterialForTexture(Ref<Texture> texture) {
    if (!texture) return nullptr;
    std::string key = std::string(texture->getPath());
    if (key.empty()) key = "gen_" + std::to_string((uintptr_t)texture.get());
    if (m_defaultMaterials.contains(key)) return m_defaultMaterials[key];
    auto material = CreateRef<PBRMaterial>(m_context); material->setAlbedoMap(texture);
    m_defaultMaterials[key] = material; return material;
}

void Renderer::render(Scene& scene) {
    auto dev = m_context.getDevice();
    (void)dev.waitForFences(1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);
    Camera* activeCamera = nullptr;
    auto camView = scene.getRegistry().view<CameraComponent>();
    for (auto entity : camView) { if (camView.get<CameraComponent>(entity).active) { activeCamera = camView.get<CameraComponent>(entity).camera.get(); break; } }
    if (!activeCamera) return;

    GlobalUBO uboData; 
    uboData.view = activeCamera->getViewMatrix(); 
    uboData.proj = activeCamera->getProjectionMatrix(); 
    uboData.camPos = glm::vec3(glm::inverse(uboData.view)[3]);
    
    // Find Sun light
    uboData.lightDir = glm::normalize(glm::vec3(1.0f, -1.0f, -1.0f));
    uboData.lightColor = glm::vec3(1.0f, 1.0f, 1.0f) * 2.0f;
    
    auto lightView = scene.getRegistry().view<LightComponent>();
    for (auto entity : lightView) {
        auto& light = lightView.get<LightComponent>(entity);
        if (light.type == LightType::Directional) {
            uboData.lightColor = light.color * light.intensity;
            // For now, directional light dir is fixed or could come from transform
            if (scene.getRegistry().all_of<TransformComponent>(entity)) {
                uboData.lightDir = scene.getRegistry().get<TransformComponent>(entity).getForward();
            }
            break;
        }
    }

    m_cameraUbos[m_currentFrame]->update(&uboData, sizeof(uboData));

    uint32_t imageIndex;
    try { imageIndex = m_swapChain->acquireNextImage(m_imageAvailableSemaphores[m_currentFrame]); } catch (...) { return; }
    (void)dev.resetFences(1, &m_inFlightFences[m_currentFrame]);

    auto& cb = m_commandBuffers[m_currentFrame];
    cb.reset({}); cb.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

    vk::Image colorImage = m_swapChain->getImage(imageIndex);
    vk::Image depthImage = m_swapChain->getDepthImage();
    cb.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests, {}, nullptr, nullptr, std::vector<vk::ImageMemoryBarrier>{
        { {}, vk::AccessFlagBits::eColorAttachmentWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, colorImage, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 } },
        { {}, vk::AccessFlagBits::eDepthStencilAttachmentWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, depthImage, { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 } }
    });

    vk::RenderingAttachmentInfo colorAttr(m_swapChain->getImageViews()[imageIndex], vk::ImageLayout::eColorAttachmentOptimal, vk::ResolveModeFlagBits::eNone, {}, vk::ImageLayout::eUndefined, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::ClearValue(vk::ClearColorValue(std::array<float, 4>{0.1f, 0.1f, 0.1f, 1.0f})));
    vk::RenderingAttachmentInfo depthAttr(m_swapChain->getDepthImageView(), vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::ResolveModeFlagBits::eNone, {}, vk::ImageLayout::eUndefined, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::ClearValue(vk::ClearDepthStencilValue(1.0f, 0)));
    cb.beginRendering({ {}, {{0, 0}, m_swapChain->getExtent()}, 1, 0, 1, &colorAttr, &depthAttr });
    cb.setViewport(0, vk::Viewport(0, 0, (float)m_swapChain->getExtent().width, (float)m_swapChain->getExtent().height, 0, 1));
    cb.setScissor(0, vk::Rect2D({0, 0}, m_swapChain->getExtent()));
    
    renderSkybox(cb, scene);

    std::vector<RenderCommand> commands;
    auto meshView = scene.getRegistry().view<MeshComponent, TransformComponent>();
    for (auto entity : meshView) {
        auto& meshComp = meshView.get<MeshComponent>(entity);
        if (!meshComp.mesh) continue;
        Ref<Material> mat = meshComp.mesh->getMaterial();
        if (!mat) { Ref<Texture> tex = meshComp.mesh->getTexture(); mat = tex ? getMaterialForTexture(tex) : nullptr; }
        if (!mat) mat = m_fallbackMaterial;
        commands.push_back({ mat->getType(), mat.get(), meshComp.mesh.get(), meshView.get<TransformComponent>(entity).getTransform() });
    }

    auto modelView = scene.getRegistry().view<ModelComponent, TransformComponent>();
    for (auto entity : modelView) {
        auto& modelComp = modelView.get<ModelComponent>(entity);
        if (!modelComp.model) continue;
        glm::mat4 t = modelView.get<TransformComponent>(entity).getTransform();
        for (const auto& mesh : modelComp.model->getMeshes()) {
            Ref<Material> mat = mesh->getMaterial();
            if (!mat) { Ref<Texture> tex = mesh->getTexture(); mat = tex ? getMaterialForTexture(tex) : nullptr; }
            if (!mat) mat = m_fallbackMaterial;
            commands.push_back({ mat->getType(), mat.get(), mesh.get(), t });
        }
    }

    std::sort(commands.begin(), commands.end());

    MaterialType currentType = static_cast<MaterialType>(-1);
    Material* currentMaterial = nullptr;
    for (const auto& cmd : commands) {
        if (cmd.type != currentType) {
            m_pipelines[cmd.type]->bind(cb);
            cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelines[cmd.type]->getLayout(), 0, 1, &m_globalDescriptorSets[m_currentFrame], 0, nullptr);
            currentType = cmd.type; currentMaterial = nullptr;
        }
        if (cmd.material != currentMaterial) {
            vk::DescriptorSet matSet = cmd.material->getDescriptorSet(m_descriptorPool, m_layouts[cmd.type]);
            cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelines[cmd.type]->getLayout(), 1, 1, &matSet, 0, nullptr);
            currentMaterial = cmd.material;
        }
        cb.pushConstants(m_pipelines[cmd.type]->getLayout(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4), &cmd.transform);
        cmd.mesh->draw(cb);
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
        auto type = MaterialType::Skybox; m_pipelines[type]->bind(cb);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelines[type]->getLayout(), 0, 1, &m_globalDescriptorSets[m_currentFrame], 0, nullptr);
        m_internalSkyboxMat->setCubemap(skyboxTex);
        vk::DescriptorSet matSet = m_internalSkyboxMat->getDescriptorSet(m_descriptorPool, m_layouts[type]);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelines[type]->getLayout(), 1, 1, &matSet, 0, nullptr);
        m_skyboxCube->draw(cb);
    }
    auto skySphereView = scene.getRegistry().view<SkySphereComponent>();
    for (auto entity : skySphereView) {
        auto& comp = skySphereView.get<SkySphereComponent>(entity);
        if (comp.texture) {
            auto type = MaterialType::SkySphere; m_pipelines[type]->bind(cb);
            cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelines[type]->getLayout(), 0, 1, &m_globalDescriptorSets[m_currentFrame], 0, nullptr);
            m_internalSkySphereMat->setTexture(comp.texture);
            vk::DescriptorSet matSet = m_internalSkySphereMat->getDescriptorSet(m_descriptorPool, m_layouts[type]);
            cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelines[type]->getLayout(), 1, 1, &matSet, 0, nullptr);
            m_skyboxCube->draw(cb);
        }
    }
}

void Renderer::onResize(int width, int height) { m_swapChain->recreate(width, height); }

} // namespace bb3d