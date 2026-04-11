#include "bb3d/render/Renderer.hpp"
#include "bb3d/render/ShadowCascade.hpp"
#include "bb3d/core/Window.hpp"
#include "bb3d/core/Log.hpp"
#include "bb3d/render/UniformBuffer.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/render/MeshGenerator.hpp"
#include <array>
#include <algorithm>
#include <stb_image_write.h>
#include <SDL3/SDL.h>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

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
    createShadowObjects();
    createGlobalDescriptors();
    createPipelines(config);
    createCopyPipeline();

    m_skyboxCube = MeshGenerator::createCube(m_context, 1.0f); 
    m_particleQuad = MeshGenerator::createQuad(m_context, 1.0f);
    m_internalSkyboxMat = CreateRef<SkyboxMaterial>(m_context);
    m_internalSkySphereMat = CreateRef<SkySphereMaterial>(m_context);
    m_defaultParticleMat = CreateRef<ParticleMaterial>(m_context);
    m_fallbackMaterial = CreateRef<PBRMaterial>(m_context);

    // Highlight Box setup (Orange color)
    m_highlightCube = MeshGenerator::createWireframeCube(m_context, 1.0f, {1.0f, 0.6f, 0.0f});
    m_highlightMat = CreateRef<UnlitMaterial>(m_context);
    m_highlightMat->setColor({1.0f, 0.6f, 0.0f});

    // Hover setup (Light Blue color)
    m_hoveredMat = CreateRef<UnlitMaterial>(m_context);
    m_hoveredMat->setColor({0.3f, 0.6f, 1.0f});

    // Debug Physics Collider setup (Green color)
    m_debugColliderMat = CreateRef<UnlitMaterial>(m_context);
    m_debugColliderMat->setColor({0.0f, 1.0f, 0.3f});
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
        m_defaultParticleMat.reset();
        m_fallbackMaterial.reset();
        m_skyboxCube.reset();
        m_particleQuad.reset();
        for (auto& ubo : m_cameraUbos) ubo.reset();
        m_cameraUbos.clear();
        for (auto& buf : m_instanceBuffers) buf.reset();
        m_instanceBuffers.clear();

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (m_imageAvailableSemaphores[i]) dev.destroySemaphore(m_imageAvailableSemaphores[i]);
            if (m_inFlightFences[i]) dev.destroyFence(m_inFlightFences[i]);
        }
        for (auto semaphore : m_renderFinishedSemaphores) if (semaphore) dev.destroySemaphore(semaphore);
        m_renderFinishedSemaphores.clear();

        if (m_highlightCube) m_highlightCube.reset();
        if (m_highlightMat) m_highlightMat.reset();
        if (m_hoveredMat) m_hoveredMat.reset();

        // Picking cleanup
        m_pickingPipeline.reset();
        m_pickingStagingBuffer.reset();
        if (m_pickingImageView) dev.destroyImageView(m_pickingImageView);
        if (m_pickingImage) vmaDestroyImage(m_context.getAllocator(), m_pickingImage, m_pickingAllocation);
        if (m_pickingDepthImageView) dev.destroyImageView(m_pickingDepthImageView);
        if (m_pickingDepthImage) vmaDestroyImage(m_context.getAllocator(), m_pickingDepthImage, m_pickingDepthAllocation);
        m_pickingInstanceBuffers.clear();
        m_pickingDescriptorSets.clear();

        if (m_descriptorPool) dev.destroyDescriptorPool(m_descriptorPool);
        if (m_shadowSampler) dev.destroySampler(m_shadowSampler);
        for (auto& cv : m_shadowCascadeViews) if (cv) dev.destroyImageView(cv);
        m_shadowCascadeViews.clear();
        if (m_shadowDepthView) dev.destroyImageView(m_shadowDepthView);
        if (m_shadowDepthImage) vmaDestroyImage(m_context.getAllocator(), m_shadowDepthImage, m_shadowDepthAllocation);
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
    m_imagesInUseFences.assign(m_swapChain->getImageCount(), nullptr);
}

void Renderer::createShadowObjects() {
    auto dev = m_context.getDevice();
    auto allocator = m_context.getAllocator();

    vk::Format depthFormat = m_swapChain->getDepthFormat(); // using standard context depth format

    uint32_t resolution = m_config.graphics.shadowsEnabled ? m_config.graphics.shadowMapResolution : 1;
    uint32_t cascades = m_config.graphics.shadowsEnabled ? m_config.graphics.shadowCascades : 4;

    // 1. Create Texture2DArray Image (layers = cascades)
    vk::ImageCreateInfo imageInfo = {};
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.extent = vk::Extent3D(resolution, resolution, 1);
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = cascades;
    imageInfo.format = depthFormat;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;
    imageInfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;
    imageInfo.samples = vk::SampleCountFlagBits::e1;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkImage img;
    vmaCreateImage(allocator, reinterpret_cast<VkImageCreateInfo*>(&imageInfo), &allocInfo, &img, &m_shadowDepthAllocation, nullptr);
    m_shadowDepthImage = img;

    // 2. Create ImageView (2D Array)
    vk::ImageViewCreateInfo viewInfo = {};
    viewInfo.image = m_shadowDepthImage;
    viewInfo.viewType = vk::ImageViewType::e2DArray;
    viewInfo.format = depthFormat;
    viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = cascades;

    m_shadowDepthView = dev.createImageView(viewInfo);

    m_shadowCascadeViews.resize(cascades);
    m_shadowCascades.resize(cascades);
    float splitLambda = 0.95f; // Practical split lambda
    float nearPlane = 0.1f; // Assuming a default near plane, adjust as needed
    float farPlane = 100.0f; // Assuming a default far plane, adjust as needed
    for (uint32_t i = 0; i < cascades; ++i) {
        float p = (float)(i + 1) / (float)cascades;
        float log = nearPlane * std::pow(farPlane / nearPlane, p);
        float lin = nearPlane + (farPlane - nearPlane) * p;
        m_shadowCascades[i].splitDepth = splitLambda * log + (1.0f - splitLambda) * lin;

        vk::ImageViewCreateInfo cascadeViewInfo = viewInfo;
        cascadeViewInfo.viewType = vk::ImageViewType::e2D;
        cascadeViewInfo.subresourceRange.baseArrayLayer = i;
        cascadeViewInfo.subresourceRange.layerCount = 1;
        m_shadowCascadeViews[i] = dev.createImageView(cascadeViewInfo);
    }

    // Rendering Dynamique : Plus besoin de RenderPass


    // 4. Create Sampler for reading in shaders
    vk::SamplerCreateInfo samplerInfo = {};
    samplerInfo.magFilter = vk::Filter::eLinear;
    samplerInfo.minFilter = vk::Filter::eLinear;
    // VERY IMPORTANT FOR PCF: Using ClampToBorder to avoid sampling outside shadow map bounds and creating artifacts
    samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToBorder;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToBorder;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToBorder;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = vk::BorderColor::eFloatOpaqueWhite; // Areas outside shadow map are lit
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_TRUE; // Enable PCF comparison
    samplerInfo.compareOp = vk::CompareOp::eLessOrEqual;
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;

    m_shadowSampler = dev.createSampler(samplerInfo);
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
        {1, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eVertex},
        {2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}
    };
    m_globalDescriptorLayout = dev.createDescriptorSetLayout({ {}, (uint32_t)bindings.size(), bindings.data() });
    std::vector<vk::DescriptorPoolSize> pSizes = { {vk::DescriptorType::eUniformBuffer, 500}, {vk::DescriptorType::eCombinedImageSampler, 1000}, {vk::DescriptorType::eStorageBuffer, 100} };
    m_descriptorPool = dev.createDescriptorPool({ vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 2000, (uint32_t)pSizes.size(), pSizes.data() });
    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_globalDescriptorLayout);
    m_globalDescriptorSets = dev.allocateDescriptorSets({ m_descriptorPool, MAX_FRAMES_IN_FLIGHT, layouts.data() });
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vk::DescriptorBufferInfo camInfo(m_cameraUbos[i]->getHandle(), 0, sizeof(GlobalUBO));
        vk::DescriptorBufferInfo instInfo(m_instanceBuffers[i]->getHandle(), 0, sizeof(glm::mat4) * MAX_INSTANCES);
        vk::DescriptorImageInfo shadowInfo(m_shadowSampler, m_shadowDepthView, vk::ImageLayout::eDepthStencilReadOnlyOptimal);
        
        std::vector<vk::WriteDescriptorSet> writes = { 
            {m_globalDescriptorSets[i], 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &camInfo}, 
            {m_globalDescriptorSets[i], 1, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &instInfo},
            {m_globalDescriptorSets[i], 2, 0, 1, vk::DescriptorType::eCombinedImageSampler, &shadowInfo, nullptr}
        };
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
    m_layouts[MaterialType::Highlight] = UnlitMaterial::CreateLayout(dev);
    m_layouts[MaterialType::Plasma] = PlasmaMaterial::CreateLayout(dev);
    m_layouts[MaterialType::Particle] = ParticleMaterial::CreateLayout(dev);

    m_shaders["shadow.vert"] = CreateScope<Shader>(m_context, "assets/shaders/shadow.vert.spv");
    m_shaders["shadow.frag"] = CreateScope<Shader>(m_context, "assets/shaders/shadow.frag.spv");
    
    // --- Pipeline Shadow ---
    vk::PushConstantRange pushConstant;
    pushConstant.stageFlags = vk::ShaderStageFlagBits::eVertex;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(glm::mat4); // lightVP

    // Shadow pipeline: depth-only, front-culling for Peter Panning fix
    m_pipelines[static_cast<MaterialType>(99)] = CreateScope<GraphicsPipeline>(
        m_context,
        vk::Format::eUndefined,  // no color attachment
        m_swapChain->getDepthFormat(),
        *m_shaders["shadow.vert"], *m_shaders["shadow.frag"],
        config,
        std::vector<vk::DescriptorSetLayout>{m_globalDescriptorLayout},
        std::vector<vk::PushConstantRange>{pushConstant},
        true, // useVertexInput
        true, // depthWrite
        vk::CompareOp::eLess,
        std::vector<uint32_t>{0}, // position only
        vk::PrimitiveTopology::eTriangleList,
        BlendMode::Opaque // no blending
    );

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
    m_shaders["plasma.frag"] = CreateScope<Shader>(m_context, "assets/shaders/plasma.frag.spv");
    m_shaders["particle.vert"] = CreateScope<Shader>(m_context, "assets/shaders/particle.vert.spv");
    m_shaders["particle.frag"] = CreateScope<Shader>(m_context, "assets/shaders/particle.frag.spv");

    vk::Format colorFmt = m_config.graphics.enableOffscreenRendering ? m_renderTarget->getColorFormat() : m_swapChain->getImageFormat();
    vk::Format depthFmt = m_config.graphics.enableOffscreenRendering ? m_renderTarget->getDepthFormat() : m_swapChain->getDepthFormat();

    auto createP = [&](MaterialType t, const std::string& v, const std::string& f, const EngineConfig& cfg, bool dWrite, vk::CompareOp op, const std::vector<uint32_t>& attr = {}, vk::PrimitiveTopology top = vk::PrimitiveTopology::eTriangleList, BlendMode blend = BlendMode::Opaque) {
        std::vector<vk::DescriptorSetLayout> ls = { m_globalDescriptorLayout, m_layouts[t] };
        return CreateScope<GraphicsPipeline>(m_context, colorFmt, depthFmt, *m_shaders[v], *m_shaders[f], cfg, ls, std::vector<vk::PushConstantRange>{}, true, dWrite, op, attr, top, blend);
    };

    m_pipelines[MaterialType::PBR] = createP(MaterialType::PBR, "pbr.vert", "pbr.frag", config, true, vk::CompareOp::eLess);
    EngineConfig envCfg = config; envCfg.rasterizer.setCullMode("None");
    
    // Standard Unlit (For 3D Models without lighting) - Opaque, Depth Write, Cull Back
    m_pipelines[MaterialType::Unlit] = createP(MaterialType::Unlit, "unlit.vert", "unlit.frag", config, true, vk::CompareOp::eLess);
    m_pipelines[MaterialType::Toon] = createP(MaterialType::Toon, "toon.vert", "toon.frag", config, true, vk::CompareOp::eLess);
    std::vector<uint32_t> envAttr = { 0, 1, 2, 3, 4 };
    m_pipelines[MaterialType::Skybox] = createP(MaterialType::Skybox, "skybox.vert", "skybox.frag", envCfg, false, vk::CompareOp::eAlways, envAttr);
    
    // SkySphere has a push constant for flipY
    std::vector<vk::PushConstantRange> skySpherePCR = { vk::PushConstantRange(vk::ShaderStageFlagBits::eFragment, 0, sizeof(int)) };
    std::vector<vk::DescriptorSetLayout> lsSkySphere = { m_globalDescriptorLayout, m_layouts[MaterialType::SkySphere] };
    m_pipelines[MaterialType::SkySphere] = CreateScope<GraphicsPipeline>(m_context, colorFmt, depthFmt, *m_shaders["skysphere.vert"], *m_shaders["skysphere.frag"], envCfg, lsSkySphere, skySpherePCR, true, false, vk::CompareOp::eAlways, envAttr, vk::PrimitiveTopology::eTriangleList);
    
    // Highlight Pipeline uses LINE_LIST topology
    m_pipelines[MaterialType::Highlight] = createP(MaterialType::Highlight, "unlit.vert", "unlit.frag", envCfg, false, vk::CompareOp::eAlways, {}, vk::PrimitiveTopology::eLineList);

    // Plasma Pipeline: Additive Blending, No Depth Write
    m_pipelines[MaterialType::Plasma] = createP(MaterialType::Plasma, "unlit.vert", "plasma.frag", envCfg, false, vk::CompareOp::eLess, {}, vk::PrimitiveTopology::eTriangleList, BlendMode::Additive);

    // Particle Pipeline: Additive Blending (best for light/fire FX with black backgrounds), No Depth Write
    m_pipelines[MaterialType::Particle] = createP(MaterialType::Particle, "particle.vert", "particle.frag", envCfg, false, vk::CompareOp::eLess, {}, vk::PrimitiveTopology::eTriangleList, BlendMode::Additive);
}

void Renderer::createCopyPipeline() {
    if (!m_config.graphics.enableOffscreenRendering) return;
    auto device = m_context.getDevice();
    if (!m_copyLayout) {
        vk::DescriptorSetLayoutBinding binding{0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment};
        m_copyLayout = device.createDescriptorSetLayout({{}, 1, &binding});
    }
    if (!m_copyPipeline) {
        EngineConfig copyConfig = m_config; copyConfig.rasterizer.setCullMode("None");
        m_copyPipeline = CreateScope<GraphicsPipeline>(m_context, m_swapChain->getImageFormat(), vk::Format::eUndefined, *m_shaders["fullscreen.vert"], *m_shaders["copy.frag"], copyConfig, std::vector<vk::DescriptorSetLayout>{m_copyLayout}, std::vector<vk::PushConstantRange>{}, false, false, vk::CompareOp::eAlways, std::vector<uint32_t>{}, vk::PrimitiveTopology::eTriangleList, BlendMode::Opaque); 
    }
    if (m_copyDescriptorSets.empty() || m_copyDescriptorSets.size() != MAX_FRAMES_IN_FLIGHT) {
        m_copyDescriptorSets.clear();
        std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_copyLayout);
        m_copyDescriptorSets = device.allocateDescriptorSets({ m_descriptorPool, MAX_FRAMES_IN_FLIGHT, layouts.data() });
        BB_CORE_TRACE("Renderer: Allocated {0} copy descriptor sets.", m_copyDescriptorSets.size());
    }
    if (m_renderTarget && !m_copyDescriptorSets.empty()) {
        for (uint32_t i = 0; i < m_copyDescriptorSets.size(); i++) {
            vk::DescriptorImageInfo imgInfo(m_renderTarget->getSampler(), m_renderTarget->getColorImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
            vk::WriteDescriptorSet write(m_copyDescriptorSets[i], 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imgInfo, nullptr, nullptr);
            device.updateDescriptorSets(write, nullptr);
        }
    }
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
    if (!m_frameStarted || !m_config.modules.enableEditor) return;
    auto& cb = m_commandBuffers[m_currentFrame];
    uint32_t imageIndex = m_swapChain->getCurrentImageIndex();
    vk::ImageView view = m_swapChain->getImageViews()[imageIndex];
    vk::Extent2D extent = m_swapChain->getExtent();
    vk::RenderingAttachmentInfo colorAttr(view, vk::ImageLayout::eColorAttachmentOptimal, vk::ResolveModeFlagBits::eNone, {}, vk::ImageLayout::eUndefined, vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore, vk::ClearValue());
    cb.beginRendering({ {}, {{0, 0}, extent}, 1, 0, 1, &colorAttr, nullptr });
    func(cb);
    cb.endRendering();
}
void Renderer::setHighlightBounds(const AABB& bounds, bool active) {
    m_highlightActive = active;
    if (active) {
        glm::vec3 center = bounds.center();
        glm::vec3 extents = bounds.size(); // Full size of the AABB
        
        // Scale the 1x1 cube to the AABB extents and translate it to the center
        m_highlightTransform = glm::translate(glm::mat4(1.0f), center) * glm::scale(glm::mat4(1.0f), extents);
    }
}

void Renderer::setHoveredBounds(const AABB& bounds, bool active) {
    m_hoveredActive = active;
    if (active) {
        glm::vec3 center = bounds.center();
        glm::vec3 extents = bounds.size();
        m_hoveredTransform = glm::translate(glm::mat4(1.0f), center) * glm::scale(glm::mat4(1.0f), extents);
    }
}

void Renderer::onResize(int width, int height) {
    if (width == 0 || height == 0) return;
    m_resizeRequested = true;
    m_pendingWidth = width;
    m_pendingHeight = height;
    BB_CORE_TRACE("Renderer: Resize requested to {}x{}", width, height);
}

bool Renderer::render(Scene& scene) {
    if (m_window.GetWidth() == 0 || m_window.GetHeight() == 0) return false;
    
    if (m_resizeRequested) {
        m_resizeRequested = false;
        m_context.getDevice().waitIdle();
        m_swapChain->recreate(m_pendingWidth, m_pendingHeight);
        m_imagesInUseFences.assign(m_swapChain->getImageCount(), nullptr);
    }

    // 1. Prepare data (Collect commands and fill instance buffers)
    prepareRenderData(scene);

    // 2. Identify active camera and setup UBO
    GlobalUBO uboData{}; // Zero-initialize to avoid rendering with garbage if update fails
    updateGlobalUBO(m_currentFrame, scene, uboData);
    BB_CORE_TRACE("Renderer: UBO updated");
    
    // 3. Begin Command Buffer
    auto dev = m_context.getDevice();
    (void)dev.waitForFences(1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);
    
    uint32_t imageIndex;
    try { imageIndex = m_swapChain->acquireNextImage(m_imageAvailableSemaphores[m_currentFrame]); } 
    catch (...) { return false; }

    if (m_imagesInUseFences[imageIndex]) (void)dev.waitForFences(1, &m_imagesInUseFences[imageIndex], VK_TRUE, UINT64_MAX);
    m_imagesInUseFences[imageIndex] = m_inFlightFences[m_currentFrame];
    (void)dev.resetFences(1, &m_inFlightFences[m_currentFrame]);

    auto& cb = m_commandBuffers[m_currentFrame];
    cb.reset({});
    cb.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
    m_frameStarted = true;

    // 4. Shadow Pass
    if (uboData.globalParams.y > 0.0f && m_config.graphics.shadowsEnabled) {
        renderShadows(cb, scene, uboData);
        BB_CORE_TRACE("Renderer: Shadows rendered");
    }

    // 5. Finalize UBO (after shadow pass might have updated cascades) and Draw Scene
    m_cameraUbos[m_currentFrame]->update(&uboData, sizeof(uboData));

    // 5b. Picking Pass (Internal)
    if (m_pickingRequested) {
        renderEntityIds(scene);
        m_pickingRequested = false; 
    }

    vk::Extent2D extent;
    vk::ImageView colorView, depthView;
    if (m_config.graphics.enableOffscreenRendering && m_renderTarget) {
        extent = m_renderTarget->getExtent();
        colorView = m_renderTarget->getColorImageView();
        depthView = m_renderTarget->getDepthImageView();
        
        vk::Image rtImage = m_renderTarget->getColorImage();
        vk::ImageMemoryBarrier rtBarrier({}, vk::AccessFlagBits::eColorAttachmentWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, rtImage, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
        cb.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, nullptr, nullptr, rtBarrier);
        
        vk::Image dImage = m_renderTarget->getDepthImage();
        vk::ImageMemoryBarrier dBarrier({}, vk::AccessFlagBits::eDepthStencilAttachmentWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, dImage, { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 });
        cb.pipelineBarrier(vk::PipelineStageFlagBits::eEarlyFragmentTests, vk::PipelineStageFlagBits::eEarlyFragmentTests, {}, nullptr, nullptr, dBarrier);
        
        drawScene(cb, scene, colorView, depthView, extent);
        
        bool editorActive = false;
#if defined(BB3D_ENABLE_EDITOR)
        editorActive = m_config.modules.enableEditor;
#endif
        if (!editorActive) {
            compositeToSwapchain(cb, imageIndex);
        } else {
            vk::Image swapImage = m_swapChain->getImage(imageIndex);
            vk::ImageMemoryBarrier swBarrier({}, vk::AccessFlagBits::eColorAttachmentWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, swapImage, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
            cb.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, nullptr, nullptr, swBarrier);
        }
        
        vk::ImageMemoryBarrier rtReadBarrier(vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eShaderRead, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, rtImage, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
        cb.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eFragmentShader, {}, nullptr, nullptr, rtReadBarrier);
    } else {
        extent = m_swapChain->getExtent();
        colorView = m_swapChain->getImageViews()[imageIndex];
        depthView = m_swapChain->getDepthImageView();
        
        vk::Image swImage = m_swapChain->getImage(imageIndex);
        vk::ImageMemoryBarrier swBarrier({}, vk::AccessFlagBits::eColorAttachmentWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, swImage, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
        cb.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, nullptr, nullptr, swBarrier);
        
        vk::Image dImage = m_swapChain->getDepthImage();
        vk::ImageMemoryBarrier dBarrier({}, vk::AccessFlagBits::eDepthStencilAttachmentWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, dImage, { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 });
        cb.pipelineBarrier(vk::PipelineStageFlagBits::eEarlyFragmentTests, vk::PipelineStageFlagBits::eEarlyFragmentTests, {}, nullptr, nullptr, dBarrier);
        
        drawScene(cb, scene, colorView, depthView, extent);
    }
    BB_CORE_TRACE("Renderer: Scene drawn");

    return true;
}

void Renderer::submitAndPresent() {
    if (!m_frameStarted) return;
    m_frameStarted = false;
    auto& cb = m_commandBuffers[m_currentFrame];
    uint32_t imageIndex = m_swapChain->getCurrentImageIndex();
    vk::Image swImage = m_swapChain->getImage(imageIndex);
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
    BB_CORE_TRACE("Renderer: Drawing scene (Frame: {})", m_currentFrame);
    
    vk::RenderingAttachmentInfo colorAttr;
    colorAttr.imageView = colorView;
    colorAttr.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttr.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttr.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttr.clearValue = vk::ClearValue(vk::ClearColorValue(std::array<float, 4>{0.1f, 0.1f, 0.1f, 1.0f}));

    vk::RenderingAttachmentInfo depthAttr;
    depthAttr.imageView = depthView;
    depthAttr.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    depthAttr.loadOp = vk::AttachmentLoadOp::eClear;
    depthAttr.storeOp = vk::AttachmentStoreOp::eStore;
    depthAttr.clearValue = vk::ClearValue(vk::ClearDepthStencilValue(1.0f, 0));

    cb.beginRendering({ {}, {{0, 0}, extent}, 1, 0, 1, &colorAttr, &depthAttr });
    cb.setViewport(0, vk::Viewport(0, 0, (float)extent.width, (float)extent.height, 0, 1));
    cb.setScissor(0, vk::Rect2D({0, 0}, extent));
    renderSkybox(cb, scene);
    
    // The commands are already sorted and the instance buffer is filled in prepareRenderData
    GraphicsPipeline* lastPipeline = nullptr; 
    Material* lastMaterial = nullptr; 
    Mesh* lastMesh = nullptr;
    
    uint32_t currentBatchStart = 0;
    uint32_t currentBatchCount = 0;

    auto flushBatch = [&]() {
        if (currentBatchCount == 0) return;
        lastMesh->draw(cb, currentBatchCount, currentBatchStart);
        currentBatchStart += currentBatchCount;
        currentBatchCount = 0;
    };

    for (uint32_t i = 0; i < (uint32_t)m_renderCommands.size(); ++i) {
        const auto& cmd = m_renderCommands[i];
        if (i >= MAX_INSTANCES) break;

        if (cmd.type == MaterialType::Skybox || cmd.type == MaterialType::SkySphere) continue;

        auto& pipeline = m_pipelines[cmd.type];
        bool pipelineChanged = false;
        if (pipeline.get() != lastPipeline) {
            flushBatch();
            pipeline->bind(cb);
            lastPipeline = pipeline.get();
            cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->getLayout(), 0, 1, &m_globalDescriptorSets[m_currentFrame], 0, nullptr);
            pipelineChanged = true;
            currentBatchStart = i; // Reset start for new pipeline/batch
        }

        if (cmd.material != lastMaterial || pipelineChanged) {
            flushBatch();
            vk::DescriptorSet ds = cmd.material->getDescriptorSet(m_descriptorPool, m_layouts[cmd.type]);
            cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->getLayout(), 1, 1, &ds, 0, nullptr);
            lastMaterial = cmd.material;
            currentBatchStart = i;
        }

        if (cmd.mesh != lastMesh) {
            flushBatch();
            lastMesh = cmd.mesh;
            currentBatchStart = i;
        }

        currentBatchCount++;
    }
    flushBatch();
    cb.endRendering();
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
            int flipY = sky.flipY ? 1 : 0;
            cb.pushConstants(pipeline->getLayout(), vk::ShaderStageFlagBits::eFragment, 0, sizeof(int), &flipY);
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
    vk::RenderingAttachmentInfo colorAttr;
    colorAttr.imageView = m_swapChain->getImageViews()[imageIndex];
    colorAttr.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttr.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttr.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttr.clearValue = vk::ClearValue(vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}));

    cb.beginRendering({ {}, {{0, 0}, m_swapChain->getExtent()}, 1, 0, 1, &colorAttr, nullptr });
    cb.setViewport(0, vk::Viewport(0, 0, (float)m_swapChain->getExtent().width, (float)m_swapChain->getExtent().height, 0, 1));
    cb.setScissor(0, vk::Rect2D({0, 0}, m_swapChain->getExtent()));
    m_copyPipeline->bind(cb);
    if (!m_copyDescriptorSets.empty() && m_copyDescriptorSets.size() > m_currentFrame) {
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_copyPipeline->getLayout(), 0, 1, &m_copyDescriptorSets[m_currentFrame], 0, nullptr);
    }
    cb.draw(3, 1, 0, 0);
    cb.endRendering();
    vk::ImageMemoryBarrier resetBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eColorAttachmentWrite, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eColorAttachmentOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, rtImage, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
    cb.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, nullptr, nullptr, resetBarrier);
}

void Renderer::updateGlobalUBO(uint32_t currentFrame, Scene& scene, GlobalUBO& uboData) {
    Camera* activeCamera = nullptr;
    auto camView = scene.getRegistry().view<CameraComponent>();
    for (auto entity : camView) {
        if (camView.get<CameraComponent>(entity).active) {
            activeCamera = camView.get<CameraComponent>(entity).camera.get();
            break;
        }
    if (!activeCamera) {
        BB_CORE_WARN("Renderer: No active camera found in scene!");
        return;
    }

    if (m_config.graphics.enableFrustumCulling) {
        m_frustum.update(activeCamera->getProjectionMatrix() * activeCamera->getViewMatrix());
    }

    uboData.view = activeCamera->getViewMatrix();
    uboData.proj = activeCamera->getProjectionMatrix();
    uboData.camPos = glm::vec4(activeCamera->getPosition(), 1.0f);
    uboData.ambientColor = glm::vec4(0.15f, 0.15f, 0.2f, 1.0f);

    int numLights = 0;
    bool directionalShadows = false;
    auto lightView = scene.getRegistry().view<LightComponent>();
    for (auto entity : lightView) {
        if (numLights >= 10) break;
        auto& light = lightView.get<LightComponent>(entity);
        ShaderLight& sl = uboData.lights[numLights];
        sl.color = glm::vec4(light.color, light.intensity);
        sl.params = glm::vec4(light.range, 0.0f, 0.0f, 0.0f);
        
        if (light.type == LightType::Directional) {
            sl.position.w = 0.0f;
            if (scene.getRegistry().all_of<TransformComponent>(entity))
                sl.direction = glm::vec4(scene.getRegistry().get<TransformComponent>(entity).getForward(), 0.0f);
            
            if (light.castShadows && !directionalShadows) {
                directionalShadows = true;
                if (numLights > 0) std::swap(uboData.lights[0], uboData.lights[numLights]);
            }
        } else {
            sl.position.w = 1.0f;
            if (scene.getRegistry().all_of<TransformComponent>(entity))
                sl.position = glm::vec4(scene.getRegistry().get<TransformComponent>(entity).translation, 1.0f);
        }
        numLights++;
    }
    uboData.globalParams = glm::vec4((float)numLights, directionalShadows ? 1.0f : 0.0f, 0.0f, 0.0f);
}

void Renderer::prepareRenderData(Scene& scene) {
    std::lock_guard<std::mutex> lock(m_commandMutex);
    m_renderCommands.clear();
    m_instanceTransforms.clear();

    // 1. Collect MeshComponent commands
    auto meshView = scene.getRegistry().view<MeshComponent, TransformComponent>();
    for (auto entity : meshView) {
        auto& meshComp = meshView.get<MeshComponent>(entity);
        if (!meshComp.mesh || !meshComp.visible) continue;
        
        RenderCommand cmd;
        auto mat = meshComp.mesh->getMaterial();
        if (!mat) mat = m_fallbackMaterial;
        
        cmd.type = mat->getType();
        cmd.material = mat.get();
        cmd.mesh = meshComp.mesh.get();
        cmd.transform = meshView.get<TransformComponent>(entity).getTransform();
        cmd.castShadows = meshComp.castShadows;
        
        m_renderCommands.push_back(cmd);
    }

    // 2. Collect ModelComponent commands
    auto modelView = scene.getRegistry().view<ModelComponent, TransformComponent>();
    for (auto entity : modelView) {
        auto& modelComp = modelView.get<ModelComponent>(entity);
        if (!modelComp.model || !modelComp.visible) continue;
        
        glm::mat4 baseTransform = modelView.get<TransformComponent>(entity).getTransform();
        glm::mat4 modelTransform = glm::translate(baseTransform, modelComp.offset);
        
        for (const auto& mesh : modelComp.model->getMeshes()) {
            if (!mesh->isVisible()) continue;
            
            RenderCommand cmd;
            auto mat = mesh->getMaterial();
            if (!mat) mat = m_fallbackMaterial;

            cmd.type = mat->getType();
            cmd.material = mat.get();
            cmd.mesh = mesh.get();
            cmd.transform = modelTransform; 
            cmd.castShadows = modelComp.castShadows;
            
            m_renderCommands.push_back(cmd);
        }
    }

    // 3. Collect ParticleSystem
    auto particleView = scene.getRegistry().view<ParticleSystemComponent>();
    for (entt::entity entity : particleView) {
        auto& particleSys = particleView.get<ParticleSystemComponent>(entity);
        Material* mat = particleSys.material ? particleSys.material.get() : m_defaultParticleMat.get();
        Mesh* mesh = particleSys.mesh ? particleSys.mesh.get() : m_particleQuad.get(); 
        if (!mesh) mesh = m_highlightCube.get();

        if (mat->getType() == MaterialType::Plasma) {
            static_cast<PlasmaMaterial*>(mat)->setTime((float)SDL_GetTicks() / 1000.0f);
        }

        for (const auto& p : particleSys.particlePool) {
            if (p.lifeRemaining <= 0.0f) continue;
            float t = 1.0f - (p.lifeRemaining / p.lifeTime);
            float currentSize = p.sizeBegin + (p.sizeEnd - p.sizeBegin) * t;
            glm::mat4 pt = glm::translate(glm::mat4(1.0f), p.position);
            pt = glm::scale(pt, glm::vec3(currentSize));
            m_renderCommands.push_back({ mat->getType(), mat, mesh, pt, false });
        }
    }
    
    if (m_highlightActive && m_highlightCube && m_highlightMat) {
        m_renderCommands.push_back({ MaterialType::Highlight, m_highlightMat.get(), m_highlightCube.get(), m_highlightTransform, false });
    }
    if (m_hoveredActive && m_highlightCube && m_hoveredMat) {
        m_renderCommands.push_back({ MaterialType::Highlight, m_hoveredMat.get(), m_highlightCube.get(), m_hoveredTransform, false });
    }

    if (m_debugPhysicsEnabled && m_highlightCube && m_debugColliderMat) {
        auto physView = scene.getRegistry().view<PhysicsComponent, TransformComponent>();
        for (auto entity : physView) {
            auto& phys = physView.get<PhysicsComponent>(entity);
            auto& tf = physView.get<TransformComponent>(entity);
            glm::vec3 colliderScale(1.0f);
            if (phys.colliderType == ColliderType::Box) colliderScale = phys.boxHalfExtents * tf.scale * 2.0f;
            else if (phys.colliderType == ColliderType::Sphere) {
                float maxS = glm::max(tf.scale.x, glm::max(tf.scale.y, tf.scale.z));
                colliderScale = glm::vec3(phys.radius * maxS * 2.0f);
            } else if (phys.colliderType == ColliderType::Capsule) {
                float rScaled = phys.radius * glm::max(tf.scale.x, tf.scale.z) * 2.0f;
                colliderScale = glm::vec3(rScaled, phys.height * tf.scale.y, rScaled);
            } else colliderScale = tf.scale;

            glm::mat4 model = glm::translate(glm::mat4(1.0f), tf.translation) * glm::toMat4(glm::quat(tf.rotation)) * glm::scale(glm::mat4(1.0f), colliderScale);
            m_renderCommands.push_back({ MaterialType::Highlight, m_debugColliderMat.get(), m_highlightCube.get(), model, false });
        }
    }

    std::ranges::sort(m_renderCommands, [](const RenderCommand& a, const RenderCommand& b) {
        if (a.type != b.type) return a.type < b.type;
        if (a.material != b.material) return a.material < b.material;
        return a.mesh < b.mesh;
    });

    void* mappedData = m_instanceBuffers[m_currentFrame]->getMappedData();
    uint32_t count = (std::min)((uint32_t)m_renderCommands.size(), MAX_INSTANCES);
    for (uint32_t i = 0; i < count; ++i) {
        void* dst = static_cast<char*>(mappedData) + (i * sizeof(glm::mat4));
        memcpy(dst, &m_renderCommands[i].transform, sizeof(glm::mat4));
    }
}

void Renderer::renderShadows(vk::CommandBuffer cb, Scene& scene, GlobalUBO& uboData) {
    if (!m_config.graphics.shadowsEnabled || uboData.globalParams.x == 0) return;

    if (uboData.lights[0].position.w >= 0.5f) return; // Not directional
    glm::vec3 lightDir = glm::normalize(glm::vec3(uboData.lights[0].direction));

    Camera* activeCamera = nullptr;
    auto camView = scene.getRegistry().view<CameraComponent>();
    for (auto entity : camView) { if (camView.get<CameraComponent>(entity).active) { activeCamera = camView.get<CameraComponent>(entity).camera.get(); break; } }
    if (!activeCamera) return;

    float nearZ = activeCamera->getNearPlane();
    float farZ = activeCamera->getFarPlane();
    auto splits = ShadowCascade::calculateSplitDistances(m_config.graphics.shadowCascades, nearZ, farZ, 0.5f);
    
    glm::mat4 camProj = activeCamera->getProjectionMatrix();
    glm::mat4 camViewMat = activeCamera->getViewMatrix();

    // Transition Depth Array to Attachment
    vk::ImageMemoryBarrier depthBarrier = {};
    depthBarrier.srcAccessMask = {};
    depthBarrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
    depthBarrier.oldLayout = vk::ImageLayout::eUndefined;
    depthBarrier.newLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    depthBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    depthBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    depthBarrier.image = m_shadowDepthImage;
    depthBarrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
    depthBarrier.subresourceRange.baseMipLevel = 0;
    depthBarrier.subresourceRange.levelCount = 1;
    depthBarrier.subresourceRange.baseArrayLayer = 0;
    depthBarrier.subresourceRange.layerCount = m_config.graphics.shadowCascades;

    cb.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eEarlyFragmentTests, {}, nullptr, nullptr, depthBarrier);

    vk::Extent2D shadowExtent(m_config.graphics.shadowMapResolution, m_config.graphics.shadowMapResolution);
    cb.setViewport(0, vk::Viewport(0, 0, (float)shadowExtent.width, (float)shadowExtent.height, 0, 1));
    cb.setScissor(0, vk::Rect2D({0, 0}, shadowExtent));
    
    auto& shadowPipeline = m_pipelines[static_cast<MaterialType>(99)];
    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, shadowPipeline->getHandle());
    cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, shadowPipeline->getLayout(), 0, 1, &m_globalDescriptorSets[m_currentFrame], 0, nullptr);

    // Dynamic depth bias
    cb.setDepthBias(1.25f, 0.0f, 1.75f);

    for (uint32_t i = 0; i < m_config.graphics.shadowCascades; ++i) {
        float minZ = (i == 0) ? nearZ : splits[i-1];
        float maxZ = splits[i];

        uboData.shadowSplitDepths[i] = maxZ; 
        glm::mat4 lightVP = ShadowCascade::calculateLightSpaceMatrix(camProj, camViewMat, lightDir, minZ, maxZ, m_config.graphics.shadowMapResolution);
        uboData.shadowCascades[i] = lightVP;

        vk::RenderingAttachmentInfo depthAttr = {};
        depthAttr.imageView = m_shadowCascadeViews[i];
        depthAttr.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
        depthAttr.loadOp = vk::AttachmentLoadOp::eClear;
        depthAttr.storeOp = vk::AttachmentStoreOp::eStore;
        depthAttr.clearValue = vk::ClearValue(vk::ClearDepthStencilValue(1.0f, 0));

        vk::RenderingInfo renderInfo = {};
        renderInfo.renderArea = vk::Rect2D({0, 0}, shadowExtent);
        renderInfo.layerCount = 1;
        renderInfo.pDepthAttachment = &depthAttr;

        cb.setViewport(0, vk::Viewport(0, 0, (float)shadowExtent.width, (float)shadowExtent.height, 0, 1));
        cb.setScissor(0, vk::Rect2D({0, 0}, shadowExtent));

        cb.beginRendering(renderInfo);
        
        cb.pushConstants(shadowPipeline->getLayout(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4), &lightVP);

        // OPTIMIZED: Use prepared commands with instancing!
        Mesh* lastMesh = nullptr;
        uint32_t batchStart = 0;
        uint32_t batchCount = 0;

        auto flushShadowBatch = [&]() {
            if (batchCount == 0) return;
            lastMesh->draw(cb, batchCount, batchStart);
            batchCount = 0;
        };

        for (uint32_t cmdIdx = 0; cmdIdx < (uint32_t)m_renderCommands.size(); ++cmdIdx) {
            const auto& cmd = m_renderCommands[cmdIdx];
            if (cmdIdx >= MAX_INSTANCES) break;
            
            // Skip non-shadow casters
            if (!cmd.castShadows) {
                flushShadowBatch();
                continue;
            }

            if (cmd.mesh != lastMesh) {
                flushShadowBatch();
                lastMesh = cmd.mesh;
                batchStart = cmdIdx;
            }
            batchCount++;
        }
        flushShadowBatch();
        cb.endRendering();
    }

    // Transition Depth Array to Shader Read
    vk::ImageMemoryBarrier depthReadBarrier = {};
    depthReadBarrier.srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
    depthReadBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
    depthReadBarrier.oldLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    depthReadBarrier.newLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
    depthReadBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    depthReadBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    depthReadBarrier.image = m_shadowDepthImage;
    depthReadBarrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
    depthReadBarrier.subresourceRange.baseMipLevel = 0;
    depthReadBarrier.subresourceRange.levelCount = 1;
    depthReadBarrier.subresourceRange.baseArrayLayer = 0;
    depthReadBarrier.subresourceRange.layerCount = m_config.graphics.shadowCascades;

    cb.pipelineBarrier(vk::PipelineStageFlagBits::eLateFragmentTests, vk::PipelineStageFlagBits::eFragmentShader, {}, nullptr, nullptr, depthReadBarrier);
}

void Renderer::createPickingResources() {
    auto dev = m_context.getDevice();
    auto allocator = m_context.getAllocator();

    // Use the same dimensions as the render target (or swapchain if no RT)
    m_pickingWidth = m_renderTarget ? m_renderTarget->getExtent().width : m_swapChain->getExtent().width;
    m_pickingHeight = m_renderTarget ? m_renderTarget->getExtent().height : m_swapChain->getExtent().height;
    m_pickingWidth = std::max(1u, m_pickingWidth);
    m_pickingHeight = std::max(1u, m_pickingHeight);

    // 1. Create R32_UINT color image for entity IDs
    VkImageCreateInfo pickImgInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    pickImgInfo.imageType = VK_IMAGE_TYPE_2D;
    pickImgInfo.format = VK_FORMAT_R32_UINT;
    pickImgInfo.extent = {m_pickingWidth, m_pickingHeight, 1};
    pickImgInfo.mipLevels = 1;
    pickImgInfo.arrayLayers = 1;
    pickImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    pickImgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    pickImgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    pickImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo pickAllocInfo{};
    pickAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkImage rawPickImg;
    vmaCreateImage(allocator, &pickImgInfo, &pickAllocInfo, &rawPickImg, &m_pickingAllocation, nullptr);
    m_pickingImage = rawPickImg;

    vk::ImageViewCreateInfo pickViewInfo({}, m_pickingImage, vk::ImageViewType::e2D, vk::Format::eR32Uint,
        {}, {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
    m_pickingImageView = dev.createImageView(pickViewInfo);

    // 2. Create depth image for picking pass (shares format with main depth)
    vk::Format depthFormat = m_renderTarget ? m_renderTarget->getDepthFormat() : m_swapChain->getDepthFormat();
    VkImageCreateInfo pickDepthInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    pickDepthInfo.imageType = VK_IMAGE_TYPE_2D;
    pickDepthInfo.format = static_cast<VkFormat>(depthFormat);
    pickDepthInfo.extent = {m_pickingWidth, m_pickingHeight, 1};
    pickDepthInfo.mipLevels = 1;
    pickDepthInfo.arrayLayers = 1;
    pickDepthInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    pickDepthInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    pickDepthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    pickDepthInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo pickDepthAllocInfo{};
    pickDepthAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkImage rawPickDepthImg;
    vmaCreateImage(allocator, &pickDepthInfo, &pickDepthAllocInfo, &rawPickDepthImg, &m_pickingDepthAllocation, nullptr);
    m_pickingDepthImage = rawPickDepthImg;

    vk::ImageViewCreateInfo pickDepthViewInfo({}, m_pickingDepthImage, vk::ImageViewType::e2D, depthFormat,
        {}, {vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1});
    m_pickingDepthImageView = dev.createImageView(pickDepthViewInfo);

    // 3. Create staging buffer for CPU readback (1 pixel = 4 bytes)
    m_pickingStagingBuffer = CreateScope<Buffer>(m_context, sizeof(uint32_t),
        vk::BufferUsageFlagBits::eTransferDst, VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT);

    // 4. Create picking pipeline
    auto pickVert = CreateScope<Shader>(m_context, "assets/shaders/picking.vert.spv");
    auto pickFrag = CreateScope<Shader>(m_context, "assets/shaders/picking.frag.spv");
    
    vk::PushConstantRange pushRange(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, sizeof(uint32_t));
    
    m_pickingPipeline = CreateScope<GraphicsPipeline>(
        m_context, vk::Format::eR32Uint, depthFormat,
        *pickVert, *pickFrag, m_config,
        std::vector<vk::DescriptorSetLayout>{m_globalDescriptorLayout},
        std::vector<vk::PushConstantRange>{pushRange},
        true,   // useVertexInput
        true,   // depthWrite 
        vk::CompareOp::eLess,
        std::vector<uint32_t>{0}, // Only position attribute
        vk::PrimitiveTopology::eTriangleList,
        BlendMode::Opaque
    );

    BB_CORE_INFO("Renderer: GPU Color Picking resources created ({}x{}).", m_pickingWidth, m_pickingHeight);

    // 5. Create picking instance buffers and descriptor sets (one per frame)
    m_pickingInstanceBuffers.clear();
    m_pickingDescriptorSets.clear();
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_pickingInstanceBuffers.push_back(CreateScope<Buffer>(m_context, 
            MAX_INSTANCES * sizeof(glm::mat4), 
            vk::BufferUsageFlagBits::eStorageBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT));
        
        vk::DescriptorSetAllocateInfo allocInfo(m_descriptorPool, 1, &m_globalDescriptorLayout);
        auto sets = dev.allocateDescriptorSets(allocInfo);
        m_pickingDescriptorSets.push_back(sets[0]);

        // Update picking descriptor set (Set 0 for picking)
        vk::DescriptorBufferInfo globalUboInfo(m_cameraUbos[i]->getHandle(), 0, VK_WHOLE_SIZE);
        vk::DescriptorBufferInfo pickingInstanceInfo(m_pickingInstanceBuffers[i]->getHandle(), 0, VK_WHOLE_SIZE);

        std::array<vk::WriteDescriptorSet, 2> pickingWrites = {
            vk::WriteDescriptorSet(m_pickingDescriptorSets[i], 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &globalUboInfo),
            vk::WriteDescriptorSet(m_pickingDescriptorSets[i], 1, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &pickingInstanceInfo)
        };
        dev.updateDescriptorSets(pickingWrites, {});
    }

    m_pickingReady = true;
}

void Renderer::renderEntityIds(Scene& scene) {
    auto& cb = m_commandBuffers[m_currentFrame];
    if (!m_pickingPipeline) {
        createPickingResources();
        if (!m_pickingPipeline) return;
    }
    if (!m_pickingReady || m_pickingInstanceBuffers.empty() || m_pickingDescriptorSets.empty()) return;

    // Resize if needed
    uint32_t targetW = m_renderTarget ? m_renderTarget->getExtent().width : m_swapChain->getExtent().width;
    uint32_t targetH = m_renderTarget ? m_renderTarget->getExtent().height : m_swapChain->getExtent().height;
    if (targetW != m_pickingWidth || targetH != m_pickingHeight) {
        // Cleanup old
        auto dev = m_context.getDevice();
        dev.waitIdle();
        if (m_pickingImageView) dev.destroyImageView(m_pickingImageView);
        if (m_pickingImage) vmaDestroyImage(m_context.getAllocator(), m_pickingImage, m_pickingAllocation);
        if (m_pickingDepthImageView) dev.destroyImageView(m_pickingDepthImageView);
        if (m_pickingDepthImage) vmaDestroyImage(m_context.getAllocator(), m_pickingDepthImage, m_pickingDepthAllocation);
        m_pickingImage = nullptr; m_pickingDepthImage = nullptr;
        m_pickingAllocation = nullptr; m_pickingDepthAllocation = nullptr;
        createPickingResources();
        if (!m_pickingPipeline) return;
    }

    // The 'cb' variable is already declared above, remove the redeclaration
    // auto& cb = m_commandBuffers[m_currentFrame];

    // Transition picking images to attachment
    vk::ImageMemoryBarrier pickBarrier({}, vk::AccessFlagBits::eColorAttachmentWrite, 
        vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, m_pickingImage,
        {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
    cb.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput, 
        {}, nullptr, nullptr, pickBarrier);

    vk::ImageMemoryBarrier pickDepthBarrier({}, vk::AccessFlagBits::eDepthStencilAttachmentWrite,
        vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, m_pickingDepthImage,
        {vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1});
    cb.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eEarlyFragmentTests,
        {}, nullptr, nullptr, pickDepthBarrier);
    BB_CORE_INFO("Renderer: Picking barriers done");

    // Begin entity ID rendering pass
    std::array<uint32_t, 4> clearIdValues = {0xFFFFFFFF, 0, 0, 0};
    vk::ClearValue clearId;
    clearId.color = vk::ClearColorValue(clearIdValues);
    vk::RenderingAttachmentInfo colorAttr(m_pickingImageView, vk::ImageLayout::eColorAttachmentOptimal,
        vk::ResolveModeFlagBits::eNone, {}, vk::ImageLayout::eUndefined,
        vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, clearId);
    vk::RenderingAttachmentInfo depthAttr(m_pickingDepthImageView, vk::ImageLayout::eDepthStencilAttachmentOptimal,
        vk::ResolveModeFlagBits::eNone, {}, vk::ImageLayout::eUndefined,
        vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
        vk::ClearValue(vk::ClearDepthStencilValue(1.0f, 0)));

    vk::Extent2D extent{m_pickingWidth, m_pickingHeight};
    cb.beginRendering({{}, {{0,0}, extent}, 1, 0, 1, &colorAttr, &depthAttr});
    cb.setViewport(0, vk::Viewport(0, 0, (float)extent.width, (float)extent.height, 0, 1));
    cb.setScissor(0, vk::Rect2D({0,0}, extent));
    BB_CORE_INFO("Renderer: Picking rendering begun");

    m_pickingPipeline->bind(cb);
    cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pickingPipeline->getLayout(), 0, 1,
        &m_pickingDescriptorSets[m_currentFrame], 0, nullptr);
    BB_CORE_INFO("Renderer: Picking pipeline bound");

    // Draw all mesh entities with their entityID as push constant
    uint32_t instanceOffset = 0;
    void* mappedInstanceData = m_pickingInstanceBuffers[m_currentFrame]->getMappedData();
    auto meshView = scene.getRegistry().view<MeshComponent, TransformComponent>();
    for (auto entity : meshView) {
        auto& meshComp = meshView.get<MeshComponent>(entity);
        if (!meshComp.mesh || !meshComp.visible) continue;
        glm::mat4 transform = meshView.get<TransformComponent>(entity).getTransform();
        
        // Write transform to dedicated picking instance SSBO
        if (instanceOffset >= MAX_INSTANCES) break;
        void* data = static_cast<char*>(mappedInstanceData) + (instanceOffset * sizeof(glm::mat4));
        memcpy(data, &transform, sizeof(glm::mat4));

        uint32_t entityId = static_cast<uint32_t>(entity);
        cb.pushConstants(m_pickingPipeline->getLayout(), 
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, sizeof(uint32_t), &entityId);
        meshComp.mesh->draw(cb, 1, instanceOffset);
        instanceOffset++;
    }
    BB_CORE_INFO("Renderer: Picking Mesh drawing done");

    // Draw all model entities
    auto modelView = scene.getRegistry().view<ModelComponent, TransformComponent>();
    for (auto entity : modelView) {
        auto& modelComp = modelView.get<ModelComponent>(entity);
        if (!modelComp.model || !modelComp.visible) continue;
        glm::mat4 baseTransform = modelView.get<TransformComponent>(entity).getTransform();
        glm::mat4 transform = glm::translate(baseTransform, modelComp.offset);

        uint32_t entityId = static_cast<uint32_t>(entity);
        cb.pushConstants(m_pickingPipeline->getLayout(),
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, sizeof(uint32_t), &entityId);

        for (const auto& mesh : modelComp.model->getMeshes()) {
            if (!mesh->isVisible()) continue;
            if (instanceOffset >= MAX_INSTANCES) break;
            void* data = static_cast<char*>(mappedInstanceData) + (instanceOffset * sizeof(glm::mat4));
            memcpy(data, &transform, sizeof(glm::mat4));
            mesh->draw(cb, 1, instanceOffset);
            instanceOffset++;
        }
    }
    BB_CORE_INFO("Renderer: Picking Model drawing done");

    cb.endRendering();

    // Transition picking image to transfer src for readback
    vk::ImageMemoryBarrier readBarrier(vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eTransferRead,
        vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, m_pickingImage,
        {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
    cb.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eTransfer,
        {}, nullptr, nullptr, readBarrier);

    m_pickingRendered = true;
}

uint32_t Renderer::readEntityIdAt(uint32_t x, uint32_t y) {
    if (!m_pickingRendered || !m_pickingImage || !m_pickingStagingBuffer) return 0xFFFFFFFF;
    if (x >= m_pickingWidth || y >= m_pickingHeight) return 0xFFFFFFFF;

    auto dev = m_context.getDevice();
    
    // We need a one-shot command to copy a single pixel from the picking image to the staging buffer
    vk::CommandBufferAllocateInfo allocInfo(m_commandPool, vk::CommandBufferLevel::ePrimary, 1);
    auto cmdBufs = dev.allocateCommandBuffers(allocInfo);
    auto cmd = cmdBufs[0];
    
    cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    // Copy single pixel from picking image to staging buffer
    vk::BufferImageCopy region;
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
    region.imageOffset = vk::Offset3D(static_cast<int32_t>(x), static_cast<int32_t>(y), 0);
    region.imageExtent = vk::Extent3D(1, 1, 1);
    
    cmd.copyImageToBuffer(m_pickingImage, vk::ImageLayout::eTransferSrcOptimal, 
        m_pickingStagingBuffer->getHandle(), region);

    cmd.end();

    vk::SubmitInfo submitInfo({}, {}, cmd, {});
    m_context.getGraphicsQueue().submit(submitInfo, {});
    m_context.getGraphicsQueue().waitIdle();

    dev.freeCommandBuffers(m_commandPool, cmdBufs);

    // Read back the value
    uint32_t entityId = 0xFFFFFFFF;
    void* mapped = m_pickingStagingBuffer->getMappedData();
    if (mapped) {
        memcpy(&entityId, mapped, sizeof(uint32_t));
    }

    m_pickingRendered = false;
    return entityId;
}

void Renderer::saveScreenshot(const std::string& filepath) {
    auto device = m_context.getDevice();
    device.waitIdle();

    uint32_t imageIndex = m_swapChain->getCurrentImageIndex();
    vk::Image srcImage = m_swapChain->getImage(imageIndex);
    vk::Extent2D extent = m_swapChain->getExtent();
    vk::Format format = m_swapChain->getImageFormat();

    vk::DeviceSize imageSize = extent.width * extent.height * 4;
    Buffer stagingBuffer(m_context, imageSize, vk::BufferUsageFlagBits::eTransferDst, VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT);

    vk::CommandBuffer cb = m_context.beginSingleTimeCommands();

    // Transition swapchain image to TransferSrc (current layout is PresentSrc)
    vk::ImageMemoryBarrier barrier({}, vk::AccessFlagBits::eTransferRead, vk::ImageLayout::ePresentSrcKHR, vk::ImageLayout::eTransferSrcOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, srcImage, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
    cb.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, nullptr, nullptr, barrier);

    vk::BufferImageCopy region(0, 0, 0, { vk::ImageAspectFlagBits::eColor, 0, 0, 1 }, { 0, 0, 0 }, { extent.width, extent.height, 1 });
    cb.copyImageToBuffer(srcImage, vk::ImageLayout::eTransferSrcOptimal, stagingBuffer.getHandle(), region);

    // Transition back to PresentSrc
    barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
    barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
    barrier.dstAccessMask = vk::AccessFlagBits::eMemoryRead;
    cb.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, nullptr, nullptr, barrier);

    m_context.endSingleTimeCommands(cb);

    // Copy to vector to free up the Vulkan buffer immediately and pass to thread
    uint8_t* mapped = static_cast<uint8_t*>(stagingBuffer.getMappedData());
    if (!mapped) {
        BB_CORE_ERROR("Renderer: Failed to map staging buffer for screenshot.");
        return;
    }
    
    std::vector<uint8_t> pixelData(mapped, mapped + imageSize);

    // Launch background job for pixel processing and saving
    m_jobSystem.executeSafe([data = std::move(pixelData), extent, format, filepath]() mutable {
        // 1. Swizzle BGRA to RGBA if format is BGR (common for Windows swapchain)
        if (format == vk::Format::eB8G8R8A8Srgb || format == vk::Format::eB8G8R8A8Unorm || format == vk::Format::eB8G8R8A8Uint) {
            for (uint32_t i = 0; i < extent.width * extent.height; ++i) {
                uint8_t temp = data[i * 4 + 0];
                data[i * 4 + 0] = data[i * 4 + 2];
                data[i * 4 + 2] = temp;
            }
        }

        // 2. Ensure directory exists if path contains directories
        std::filesystem::path p(filepath);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }

        // 3. Save as PNG
        if (stbi_write_png(filepath.c_str(), extent.width, extent.height, 4, data.data(), extent.width * 4)) {
            BB_CORE_INFO("Screenshot saved asynchronously to: {0}", filepath);
        } else {
            BB_CORE_ERROR("Failed to save screenshot asynchronously to: {0}", filepath);
        }
    });
}

} // namespace bb3d
