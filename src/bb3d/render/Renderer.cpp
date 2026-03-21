#include "bb3d/render/Renderer.hpp"
#include "bb3d/render/ShadowCascade.hpp"
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
    for (uint32_t i = 0; i < cascades; ++i) {
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
    if (m_window.GetWidth() == 0 || m_window.GetHeight() == 0) {
        BB_CORE_WARN("Renderer::render: Skipping render due to invalid window dimensions.");
        return false;
    }
    if (m_resizeRequested) {
        m_resizeRequested = false;
        try {
            auto capabilities = m_context.getPhysicalDevice().getSurfaceCapabilitiesKHR(m_context.getSurface());
            if (capabilities.currentExtent.width > 0 && capabilities.currentExtent.height > 0) {
                m_context.getDevice().waitIdle();
                m_swapChain->recreate(m_pendingWidth, m_pendingHeight);
                m_imagesInUseFences.assign(m_swapChain->getImageCount(), nullptr);
                auto dev = m_context.getDevice();
                for (auto s : m_renderFinishedSemaphores) if (s) dev.destroySemaphore(s);
                m_renderFinishedSemaphores.clear(); 
                m_renderFinishedSemaphores.resize(m_swapChain->getImageCount());
                for (size_t i = 0; i < m_renderFinishedSemaphores.size(); i++) 
                    m_renderFinishedSemaphores[i] = dev.createSemaphore({});
                if (m_config.graphics.enableOffscreenRendering) {
                    bool editorEnabled = false;
#if defined(BB3D_ENABLE_EDITOR)
                    editorEnabled = m_config.modules.enableEditor;
#endif
                    // In Editor mode, the RenderTarget is resized by the Viewport panel logic in Engine/ImGuiLayer.
                    // Resizing here based on Window size would conflict and cause flickering.
                    if (!editorEnabled) {
                        if (!m_renderTarget) {
                            uint32_t w = static_cast<uint32_t>(m_pendingWidth * m_config.graphics.renderScale);
                            uint32_t h = static_cast<uint32_t>(m_pendingHeight * m_config.graphics.renderScale);
                            w = std::max(1u, w); h = std::max(1u, h);
                            m_renderTarget = CreateScope<RenderTarget>(m_context, w, h);
                        } else {
                            uint32_t w = static_cast<uint32_t>(m_pendingWidth * m_config.graphics.renderScale);
                            uint32_t h = static_cast<uint32_t>(m_pendingHeight * m_config.graphics.renderScale);
                            w = std::max(1u, w); h = std::max(1u, h);
                            m_renderTarget->resize(w, h);
                        }
                    }

                    if (m_renderTarget) {
                        if (m_copyDescriptorSets.size() != MAX_FRAMES_IN_FLIGHT) {
                            createCopyPipeline();
                        } else {
                            for (uint32_t i = 0; i < m_copyDescriptorSets.size(); i++) {
                                vk::DescriptorImageInfo imgInfo(m_renderTarget->getSampler(), m_renderTarget->getColorImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
                                vk::WriteDescriptorSet write(m_copyDescriptorSets[i], 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imgInfo, nullptr, nullptr);
                                dev.updateDescriptorSets(write, nullptr);
                            }
                        }
                    }
                }
                BB_CORE_INFO("Renderer: Deferred resize applied ({}x{})", m_pendingWidth, m_pendingHeight);
            }
        } catch (const std::exception& e) {
            BB_CORE_ERROR("Renderer: Failed to apply deferred resize: {}", e.what());
            return false;
        }
    }
    Material::SetCurrentFrame(m_currentFrame);
    m_frameStarted = false;
    auto dev = m_context.getDevice();
    (void)dev.waitForFences(1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);
    Camera* activeCamera = nullptr;
    auto camView = scene.getRegistry().view<CameraComponent>();
    for (auto entity : camView) { if (camView.get<CameraComponent>(entity).active) { activeCamera = camView.get<CameraComponent>(entity).camera.get(); break; } }
    if (!activeCamera) return false;
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
    
    
    // NOTE: renderShadows requires a command buffer. It is called after cb.begin() below.

    uboData.globalParams = glm::vec4((float)numLights, 0.0f, 0.0f, 0.0f);
    uboData.ambientColor = glm::vec4(0.15f, 0.15f, 0.2f, 1.0f); // Default ambient, slightly blueish
    m_cameraUbos[m_currentFrame]->update(&uboData, sizeof(uboData));
    uint32_t imageIndex;
    try { imageIndex = m_swapChain->acquireNextImage(m_imageAvailableSemaphores[m_currentFrame]); } 
    catch (...) { onResize(m_window.GetWidth(), m_window.GetHeight()); return false; }
    if (m_imagesInUseFences[imageIndex]) (void)dev.waitForFences(1, &m_imagesInUseFences[imageIndex], VK_TRUE, UINT64_MAX);
    m_imagesInUseFences[imageIndex] = m_inFlightFences[m_currentFrame];
    (void)dev.resetFences(1, &m_inFlightFences[m_currentFrame]);
    auto& cb = m_commandBuffers[m_currentFrame];
    cb.reset({}); cb.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
    m_frameStarted = true;

    // Render Shadows (Updates uboData.shadowCascades) - must be done after cb.begin()
    renderShadows(cb, scene, uboData);
    // Re-upload UBO after shadow pass updated the cascade matrices
    m_cameraUbos[m_currentFrame]->update(&uboData, sizeof(uboData));

    if (m_config.graphics.enableOffscreenRendering) {
        vk::Image rtImage = m_renderTarget->getColorImage();
        vk::ImageMemoryBarrier rtBarrier({}, vk::AccessFlagBits::eColorAttachmentWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, rtImage, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
        cb.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, nullptr, nullptr, rtBarrier);
        vk::Image dImage = m_renderTarget->getDepthImage();
        vk::ImageMemoryBarrier dBarrier({}, vk::AccessFlagBits::eDepthStencilAttachmentWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, dImage, { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 });
        cb.pipelineBarrier(vk::PipelineStageFlagBits::eEarlyFragmentTests, vk::PipelineStageFlagBits::eEarlyFragmentTests, {}, nullptr, nullptr, dBarrier);
        drawScene(cb, scene, m_renderTarget->getColorImageView(), m_renderTarget->getDepthImageView(), m_renderTarget->getExtent());
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
        vk::Image swImage = m_swapChain->getImage(imageIndex);
        vk::ImageMemoryBarrier swBarrier({}, vk::AccessFlagBits::eColorAttachmentWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, swImage, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
        cb.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, nullptr, nullptr, swBarrier);
        vk::Image dImage = m_swapChain->getDepthImage();
        vk::ImageMemoryBarrier dBarrier({}, vk::AccessFlagBits::eDepthStencilAttachmentWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, dImage, { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 });
        cb.pipelineBarrier(vk::PipelineStageFlagBits::eEarlyFragmentTests, vk::PipelineStageFlagBits::eEarlyFragmentTests, {}, nullptr, nullptr, dBarrier);
        drawScene(cb, scene, m_swapChain->getImageViews()[imageIndex], m_swapChain->getDepthImageView(), m_swapChain->getExtent());
    }
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
    vk::RenderingAttachmentInfo colorAttr(colorView, vk::ImageLayout::eColorAttachmentOptimal, vk::ResolveModeFlagBits::eNone, {}, vk::ImageLayout::eUndefined, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::ClearValue(vk::ClearColorValue(std::array<float, 4>{0.1f, 0.1f, 0.1f, 1.0f})));
    vk::RenderingAttachmentInfo depthAttr(depthView, vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::ResolveModeFlagBits::eNone, {}, vk::ImageLayout::eUndefined, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::ClearValue(vk::ClearDepthStencilValue(1.0f, 0)));
    cb.beginRendering({ {}, {{0, 0}, extent}, 1, 0, 1, &colorAttr, &depthAttr });
    cb.setViewport(0, vk::Viewport(0, 0, (float)extent.width, (float)extent.height, 0, 1));
    cb.setScissor(0, vk::Rect2D({0, 0}, extent));
    renderSkybox(cb, scene);
    m_renderCommands.clear();
    std::vector<entt::entity> meshEntities;
    auto meshView = scene.getRegistry().view<MeshComponent, TransformComponent>();
    meshEntities.assign(meshView.begin(), meshView.end());
    std::vector<entt::entity> modelEntities;
    auto modelView = scene.getRegistry().view<ModelComponent, TransformComponent>();
    modelEntities.assign(modelView.begin(), modelView.end());
    if (!meshEntities.empty()) {
        m_jobSystem.dispatch((uint32_t)meshEntities.size(), 64, [&](uint32_t index, uint32_t count) {
            std::vector<RenderCommand> localCommands;
            for (uint32_t i = index; i < index + count; ++i) {
                entt::entity entity = meshEntities[i];
                auto& meshComp = meshView.get<MeshComponent>(entity);
                if (!meshComp.mesh || !meshComp.visible) continue;
                glm::mat4 transform = meshView.get<TransformComponent>(entity).getTransform();
                if (m_config.graphics.enableFrustumCulling) {
                    AABB worldBox = meshComp.mesh->getBounds().transform(transform);
                    if (!m_frustum.intersects(worldBox)) continue;
                }
                Material* mat = meshComp.mesh->getMaterial().get();
                if (!mat) mat = m_fallbackMaterial.get();
                localCommands.push_back({ mat->getType(), mat, meshComp.mesh.get(), transform });
            }
            if (!localCommands.empty()) {
                std::lock_guard<std::mutex> lock(m_commandMutex);
                m_renderCommands.insert(m_renderCommands.end(), localCommands.begin(), localCommands.end());
            }
        });
    }
    if (!modelEntities.empty()) {
        m_jobSystem.dispatch((uint32_t)modelEntities.size(), 32, [&](uint32_t index, uint32_t count) {
            std::vector<RenderCommand> localCommands;
            for (uint32_t i = index; i < index + count; ++i) {
                entt::entity entity = modelEntities[i];
                auto& modelComp = modelView.get<ModelComponent>(entity);
                if (!modelComp.model || !modelComp.visible) continue;
                glm::mat4 baseTransform = modelView.get<TransformComponent>(entity).getTransform();
                glm::mat4 transform = glm::translate(baseTransform, modelComp.offset);
                
                if (m_config.graphics.enableFrustumCulling) {
                    AABB worldBox = modelComp.model->getBounds().transform(transform);
                    if (!m_frustum.intersects(worldBox)) continue;
                }
                for (const auto& mesh : modelComp.model->getMeshes()) {
                    if (!mesh->isVisible()) continue;
                    Material* mat = mesh->getMaterial().get();
                    if (!mat) {
                        Ref<Texture> tex = mesh->getTexture();
                        mat = tex ? getMaterialForTexture(tex).get() : nullptr;
                    }
                    if (!mat) mat = m_fallbackMaterial.get();
                    localCommands.push_back({ mat->getType(), mat, mesh.get(), transform });
                }
            }
            if (!localCommands.empty()) {
                std::lock_guard<std::mutex> lock(m_commandMutex);
                m_renderCommands.insert(m_renderCommands.end(), localCommands.begin(), localCommands.end());
            }
        });
    }
    
    // --- SYSTEM: Particles Rendering ---
    auto particleView = scene.getRegistry().view<ParticleSystemComponent>();
    std::vector<entt::entity> particleEntities;
    particleEntities.assign(particleView.begin(), particleView.end());
    if (!particleEntities.empty()) {
        std::vector<RenderCommand> localCommands;
        for (entt::entity entity : particleEntities) {
            auto& particleSys = particleView.get<ParticleSystemComponent>(entity);
            Material* mat = particleSys.material ? particleSys.material.get() : m_defaultParticleMat.get();
            Mesh* mesh = particleSys.mesh ? particleSys.mesh.get() : m_particleQuad.get(); 
            
            if (!mesh) mesh = m_highlightCube.get(); // Ultimate fallback

            if (mat->getType() == MaterialType::Plasma) {
                static_cast<PlasmaMaterial*>(mat)->setTime((float)SDL_GetTicks() / 1000.0f);
            }

            for (const auto& p : particleSys.particlePool) {
                if (p.lifeRemaining <= 0.0f) continue;
                
                float t = 1.0f - (p.lifeRemaining / p.lifeTime);
                float currentSize = p.sizeBegin + (p.sizeEnd - p.sizeBegin) * t;
                
                glm::mat4 pt = glm::translate(glm::mat4(1.0f), p.position);
                pt = glm::scale(pt, glm::vec3(currentSize));
                
                localCommands.push_back({ mat->getType(), mat, mesh, pt });
            }
        }
        if (!localCommands.empty()) {
            std::lock_guard<std::mutex> lock(m_commandMutex);
            m_renderCommands.insert(m_renderCommands.end(), localCommands.begin(), localCommands.end());
        }
    }
    
    // Add Highlight as a render command so it uses the standard SSBO transform logic
    if (m_highlightActive && m_highlightCube && m_highlightMat) {
        m_renderCommands.push_back({ MaterialType::Highlight, m_highlightMat.get(), m_highlightCube.get(), m_highlightTransform });
    }
    
    // Add Hover as a render command 
    if (m_hoveredActive && m_highlightCube && m_hoveredMat) {
        m_renderCommands.push_back({ MaterialType::Highlight, m_hoveredMat.get(), m_highlightCube.get(), m_hoveredTransform });
    }

    std::ranges::sort(m_renderCommands, [](const RenderCommand& a, const RenderCommand& b) {
        if (a.type != b.type) return a.type < b.type;
        if (a.material != b.material) return a.material < b.material;
        return a.mesh < b.mesh;
    });
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
    flushInstances(); 
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
    vk::RenderingAttachmentInfo colorAttr(m_swapChain->getImageViews()[imageIndex], vk::ImageLayout::eColorAttachmentOptimal, vk::ResolveModeFlagBits::eNone, {}, vk::ImageLayout::eUndefined, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::ClearValue(vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f})));
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

        cb.beginRendering(renderInfo);
        
        cb.pushConstants(shadowPipeline->getLayout(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4), &lightVP);

        // Simple iteration over all meshes to draw them purely with instances buffers
        auto meshView = scene.getRegistry().view<MeshComponent, TransformComponent>();
        uint32_t instanceOffset = 0;
        for (auto entity : meshView) {
            auto& meshComp = meshView.get<MeshComponent>(entity);
            if (!meshComp.mesh || !meshComp.visible || !meshComp.castShadows) continue;
            // Draw
            meshComp.mesh->draw(cb, 1, instanceOffset);
            instanceOffset++;
        }
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

} // namespace bb3d
