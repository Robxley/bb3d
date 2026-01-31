#include "bb3d/render/Renderer.hpp"
#include "bb3d/core/Window.hpp"
#include "bb3d/core/Log.hpp"
#include "bb3d/render/UniformBuffer.hpp"
#include "bb3d/scene/Components.hpp"
#include <array>
#include <filesystem>
#include <SDL3/SDL.h>

namespace bb3d {

Renderer::Renderer(VulkanContext& context, Window& window, const EngineConfig& config)
    : m_context(context), m_window(window) {
    
    m_swapChain = CreateScope<SwapChain>(context, config.window.width, config.window.height);
    
    createSyncObjects();
    createGlobalDescriptors();

    // Chargement shaders PBR par défaut avec vérification
    const std::string vertPath = "assets/shaders/pbr.vert.spv";
    const std::string fragPath = "assets/shaders/pbr.frag.spv";

    if (!std::filesystem::exists(vertPath)) {
        BB_CORE_ERROR("Renderer: Vertex shader not found at {}", vertPath);
        throw std::runtime_error("Missing vertex shader");
    }
    if (!std::filesystem::exists(fragPath)) {
        BB_CORE_ERROR("Renderer: Fragment shader not found at {}", fragPath);
        throw std::runtime_error("Missing fragment shader");
    }

    m_defaultVert = CreateScope<Shader>(context, vertPath);
    m_defaultFrag = CreateScope<Shader>(context, fragPath);
    
    vk::PushConstantRange pcr(vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4));
    m_defaultPipeline = CreateScope<GraphicsPipeline>(context, *m_swapChain, *m_defaultVert, *m_defaultFrag, config, std::vector<vk::DescriptorSetLayout>{m_globalDescriptorLayout, m_materialLayout}, std::vector<vk::PushConstantRange>{pcr});
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

        m_defaultMaterials.clear();
        
        if (m_descriptorPool) dev.destroyDescriptorPool(m_descriptorPool);
        if (m_globalDescriptorLayout) dev.destroyDescriptorSetLayout(m_globalDescriptorLayout);
        if (m_materialLayout) dev.destroyDescriptorSetLayout(m_materialLayout);
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
    
    // Set 0: Camera UBO
    m_cameraUbo = CreateScope<UniformBuffer>(m_context, sizeof(GlobalUBO));

    vk::DescriptorSetLayoutBinding cameraBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
    m_globalDescriptorLayout = dev.createDescriptorSetLayout({ {}, 1, &cameraBinding });

    vk::DescriptorPoolSize poolSize(vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT);
    m_descriptorPool = dev.createDescriptorPool({ {}, MAX_FRAMES_IN_FLIGHT, 1, &poolSize });

    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_globalDescriptorLayout);
    m_globalDescriptorSets = dev.allocateDescriptorSets({ m_descriptorPool, MAX_FRAMES_IN_FLIGHT, layouts.data() });

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vk::DescriptorBufferInfo bufferInfo(m_cameraUbo->getHandle(), 0, sizeof(GlobalUBO));
        vk::WriteDescriptorSet write(m_globalDescriptorSets[i], 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &bufferInfo);
        dev.updateDescriptorSets(1, &write, 0, nullptr);
    }

    // Set 1: Material (PBR)
    std::vector<vk::DescriptorSetLayoutBinding> bindings = {
        {0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}, // Albedo
        {1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}, // Normal
        {2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}, // Metallic
        {3, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}, // Roughness
        {4, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}, // AO
        {5, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}, // Emissive
    };
    m_materialLayout = dev.createDescriptorSetLayout({ {}, (uint32_t)bindings.size(), bindings.data() });

    // Pool global (assez grand pour UBOs + Materials)
    std::vector<vk::DescriptorPoolSize> poolSizes = {
        {vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT * 10},
        {vk::DescriptorType::eCombinedImageSampler, 1000 * 5} 
    };
    m_descriptorPool = dev.createDescriptorPool({ vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1000, (uint32_t)poolSizes.size(), poolSizes.data() });
}

Ref<Material> Renderer::getMaterialForTexture(Ref<Texture> texture) {
    if (!texture) return nullptr;

    if (m_defaultMaterials.contains(texture.get())) {
        return m_defaultMaterials[texture.get()];
    }

    auto material = CreateRef<Material>(m_context);
    material->setAlbedoMap(texture);
    m_defaultMaterials[texture.get()] = material;
    return material;
}

void Renderer::render(Scene& scene) {
    auto dev = m_context.getDevice();
    
    // Attendre la frame
    (void)dev.waitForFences(1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);
    
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
    uboData.camPos = glm::vec3(glm::inverse(uboData.view)[3]); // Position caméra monde
    m_cameraUbo->update(&uboData, sizeof(uboData));

    uint32_t imageIndex;
    try {
        imageIndex = m_swapChain->acquireNextImage(m_imageAvailableSemaphores[m_currentFrame]);
    } catch (const vk::OutOfDateKHRError&) {
        int w, h;
        SDL_GetWindowSize(m_window.GetNativeWindow(), &w, &h);
        onResize(w, h);
        return; // Passer cette frame, swapchain recréée
    } catch (const vk::SystemError& e) {
        BB_CORE_ERROR("Vulkan Error in acquireNextImage: {}", e.what());
        return;
    } catch (...) {
        BB_CORE_ERROR("Unknown Error in acquireNextImage");
        return;
    }

    (void)dev.resetFences(1, &m_inFlightFences[m_currentFrame]);

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
    
    // Bind Global Set (0)
    cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_defaultPipeline->getLayout(), 0, 1, &m_globalDescriptorSets[m_currentFrame], 0, nullptr);

    // Dessiner toutes les entités avec MeshComponent
    auto meshView = scene.getRegistry().view<MeshComponent, TransformComponent>();
    for (auto entity : meshView) {
        auto& meshComp = meshView.get<MeshComponent>(entity);
        auto& transform = meshView.get<TransformComponent>(entity);

        if (meshComp.mesh) {
            Ref<Material> mat = meshComp.mesh->getMaterial();
            if (!mat) {
                Ref<Texture> tex = meshComp.mesh->getTexture();
                if (tex) mat = getMaterialForTexture(tex);
                else {
                    static Ref<Material> defaultMat = nullptr;
                    if (!defaultMat) defaultMat = CreateRef<Material>(m_context);
                    mat = defaultMat;
                }
            }

            vk::DescriptorSet matSet = mat->getDescriptorSet(m_materialLayout, m_descriptorPool);
            cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_defaultPipeline->getLayout(), 1, 1, &matSet, 0, nullptr);

            glm::mat4 modelMat = transform.getTransform();
            cb.pushConstants(m_defaultPipeline->getLayout(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4), &modelMat);
            meshComp.mesh->draw(cb);
        }
    }

    // Dessiner toutes les entités avec ModelComponent
    auto modelView = scene.getRegistry().view<ModelComponent, TransformComponent>();
    for (auto entity : modelView) {
        auto& modelComp = modelView.get<ModelComponent>(entity);
        auto& transform = modelView.get<TransformComponent>(entity);

        if (modelComp.model) {
            glm::mat4 modelMat = transform.getTransform();
            cb.pushConstants(m_defaultPipeline->getLayout(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4), &modelMat);
            
            for (const auto& mesh : modelComp.model->getMeshes()) {
                Ref<Material> mat = mesh->getMaterial();
                if (!mat) {
                    Ref<Texture> tex = mesh->getTexture();
                    if (tex) mat = getMaterialForTexture(tex);
                    else {
                        static Ref<Material> defaultMat = nullptr;
                        if (!defaultMat) defaultMat = CreateRef<Material>(m_context);
                        mat = defaultMat;
                    }
                }

                vk::DescriptorSet matSet = mat->getDescriptorSet(m_materialLayout, m_descriptorPool);
                cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_defaultPipeline->getLayout(), 1, 1, &matSet, 0, nullptr);
                
                mesh->draw(cb);
            }
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

    try {
        m_swapChain->present(m_renderFinishedSemaphores[m_currentFrame], imageIndex);
    } catch (const vk::OutOfDateKHRError&) {
        int w, h;
        SDL_GetWindowSize(m_window.GetNativeWindow(), &w, &h);
        onResize(w, h);
    } catch (const vk::SystemError& e) {
        BB_CORE_ERROR("Vulkan Error in present: {}", e.what());
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::onResize(int width, int height) {
    m_swapChain->recreate(width, height);
}

} // namespace bb3d