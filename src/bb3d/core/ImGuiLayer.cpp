#include "bb3d/core/ImGuiLayer.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/core/Window.hpp"
#include "bb3d/core/Log.hpp"
#include "bb3d/core/IconsFontAwesome6.h"

#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <imgui_freetype.h>

#include <SDL3/SDL.h>
#include <filesystem>
#include <iostream>

namespace bb3d {

ImGuiLayer::ImGuiLayer(VulkanContext& context, Window& window)
    : m_context(context), m_window(window) {
    
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    
    ImGui::StyleColorsDark();

    std::vector<vk::DescriptorPoolSize> poolSizes = {
        {vk::DescriptorType::eCombinedImageSampler, 500}
    };
    m_descriptorPool = m_context.getDevice().createDescriptorPool({
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 500, (uint32_t)poolSizes.size(), poolSizes.data()
    });

    ImGui_ImplSDL3_InitForVulkan(m_window.GetNativeWindow());

    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance = m_context.getInstance();
    initInfo.PhysicalDevice = m_context.getPhysicalDevice();
    initInfo.Device = m_context.getDevice();
    initInfo.QueueFamily = m_context.getGraphicsQueueFamily();
    initInfo.Queue = m_context.getGraphicsQueue();
    initInfo.DescriptorPool = m_descriptorPool;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = 3; 
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.UseDynamicRendering = true;
    
    initInfo.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    VkFormat colorFormat = static_cast<VkFormat>(vk::Format::eB8G8R8A8Srgb); 
    initInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    initInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;

    ImGui_ImplVulkan_Init(&initInfo);

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
    // On force l'usage de FreeType avec support couleur
    io.Fonts->FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;

    const float fontSize = 18.0f;
    const float emojiSize = 24.0f;

    // --- 1. ROBOTO (BASE) ---
    // Cette police sera utilisée pour TOUT le texte de l'éditeur par défaut
    const char* robotoPath = "assets/fonts/Roboto-Regular.ttf";
    if (std::filesystem::exists(robotoPath)) {
        m_fontRoboto = io.Fonts->AddFontFromFileTTF(robotoPath, fontSize);
        BB_CORE_INFO("ImGui: Roboto loaded as base font.");
    } else {
        m_fontRoboto = io.Fonts->AddFontDefault();
        BB_CORE_WARN("ImGui: Roboto not found, using internal fixed font.");
    }

    // --- 2. FONTAWESOME (ICONS MERGE) ---
    ImFontConfig iconConfig;
    iconConfig.MergeMode = true;
    iconConfig.PixelSnapH = true;
    iconConfig.GlyphMinAdvanceX = fontSize;
    static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    const char* faPath = "assets/fonts/fa-solid-900.ttf";
    if (std::filesystem::exists(faPath)) {
        m_fontAwesome = io.Fonts->AddFontFromFileTTF(faPath, fontSize, &iconConfig, icon_ranges);
        BB_CORE_INFO("ImGui: FontAwesome merged into base font.");
    }

    // --- 3. SEGOE UI EMOJI (COLOR MERGE) ---
    // On la fusionne aussi dans la police par défaut pour que les emojis marchent partout
    ImFontConfig emojiConfig;
    emojiConfig.MergeMode = true;
    emojiConfig.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;
    static const ImWchar emoji_ranges[] = { 0x1, 0x1FFFF, 0 };
    const char* segoePath = "C:/Windows/Fonts/seguiemj.ttf";
    if (std::filesystem::exists(segoePath)) {
        m_fontSegoe = io.Fonts->AddFontFromFileTTF(segoePath, emojiSize, &emojiConfig, emoji_ranges);
        BB_CORE_INFO("ImGui: Segoe UI Emoji merged into base font.");
    }

    // On build l'atlas final (Roboto + Icons + Emojis)
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