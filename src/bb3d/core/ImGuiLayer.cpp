#include "bb3d/core/ImGuiLayer.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/core/Window.hpp"
#include "bb3d/core/Log.hpp"

#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <imgui_freetype.h>

#include <SDL3/SDL.h>

namespace bb3d {

ImGuiLayer::ImGuiLayer(VulkanContext& context, Window& window)
    : m_context(context), m_window(window) {
    
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // Note: Viewports multiples (multi-windows) sont plus complexes à synchroniser avec Vulkan/Dynamic Rendering, 
    // on reste sur du docking interne pour l'instant.
    
    ImGui::StyleColorsDark();

    // 1. Créer un DescriptorPool dédié pour ImGui
    std::vector<vk::DescriptorPoolSize> poolSizes = {
        {vk::DescriptorType::eCombinedImageSampler, 10}
    };
    m_descriptorPool = m_context.getDevice().createDescriptorPool({
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 10, (uint32_t)poolSizes.size(), poolSizes.data()
    });

    // 2. Init SDL3 Backend
    ImGui_ImplSDL3_InitForVulkan(m_window.GetSDLWindow());

    // 3. Init Vulkan Backend
    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance = m_context.getInstance();
    initInfo.PhysicalDevice = m_context.getPhysicalDevice();
    initInfo.Device = m_context.getDevice();
    initInfo.QueueFamily = m_context.getGraphicsQueueFamily();
    initInfo.Queue = m_context.getGraphicsQueue();
    initInfo.PipelineCache = nullptr;
    initInfo.DescriptorPool = m_descriptorPool;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = 3; 
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.UseDynamicRendering = true;
    
    // Configuration du Dynamic Rendering : Utiliser le format SRGB standard du moteur
    initInfo.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    VkFormat colorFormat = static_cast<VkFormat>(vk::Format::eB8G8R8A8Srgb); 
    initInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    initInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;

    ImGui_ImplVulkan_Init(&initInfo);

    // 4. Charger les polices (avec Emojis)
    initFonts();
}

ImGuiLayer::~ImGuiLayer() {
    m_context.getDevice().waitIdle();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    m_context.getDevice().destroyDescriptorPool(m_descriptorPool);
}

void ImGuiLayer::initFonts() {
    ImGuiIO& io = ImGui::GetIO();
    
    // On utilise FreeType pour le rendu (requis pour les emojis couleur)
    io.Fonts->FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;

    // 1. Charger la police principale (Arial ou Roboto)
    // On essaye de trouver une police système ou on utilise celle par défaut
    io.Fonts->AddFontDefault();

    // 2. Fusionner avec Segoe UI Emoji (Windows)
    // Utilisation de static_cast pour éviter les warnings de troncation
    static const ImWchar emoji_ranges[] = { 
        static_cast<ImWchar>(0x0001), 
        static_cast<ImWchar>(0xFFFF), // ImWchar est souvent 16-bit, limiter à 0xFFFF
        0 
    };
    ImFontConfig config;
    config.MergeMode = true;
    config.OversampleH = 1;
    config.OversampleV = 1;
    config.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;

    // Chemin Windows standard
    const char* emojiPath = "C:/Windows/Fonts/seguiemj.ttf";
    if (std::filesystem::exists(emojiPath)) {
        io.Fonts->AddFontFromFileTTF(emojiPath, 16.0f, &config, emoji_ranges);
        BB_CORE_INFO("ImGui: Color Emojis font loaded successfully.");
    } else {
        BB_CORE_WARN("ImGui: Segoe UI Emoji font not found. Color emojis will not be available.");
    }

    // Builder le atlas
    ImGui_ImplVulkan_CreateFontsTexture();
}

void ImGuiLayer::beginFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::endFrame(vk::CommandBuffer commandBuffer) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), static_cast<VkCommandBuffer>(commandBuffer));
}

void ImGuiLayer::onEvent(const SDL_Event& event) {
    ImGui_ImplSDL3_ProcessEvent(reinterpret_cast<const ::SDL_Event*>(&event));
}

bool ImGuiLayer::wantCaptureMouse() const {
    return ImGui::GetIO().WantCaptureMouse;
}

bool ImGuiLayer::wantCaptureKeyboard() const {
    return ImGui::GetIO().WantCaptureKeyboard;
}

ImTextureID ImGuiLayer::addTexture(vk::Sampler sampler, vk::ImageView view, vk::ImageLayout layout) {
    return (ImTextureID)ImGui_ImplVulkan_AddTexture(sampler, view, static_cast<VkImageLayout>(layout));
}

} // namespace bb3d
