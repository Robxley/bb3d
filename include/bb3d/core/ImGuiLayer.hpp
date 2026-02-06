#pragma once

#include "bb3d/core/Base.hpp"
#include <vulkan/vulkan.hpp>
#include <imgui.h>

struct SDL_Window;
union SDL_Event;

namespace bb3d {

class VulkanContext;
class Window;

/**
 * @brief Couche d'interface utilisateur utilisant Dear ImGui.
 * 
 * Cette classe gère l'initialisation, le rendu et les événements d'ImGui.
 * Elle est conçue pour être optionnelle et peut être désactivée en Release.
 */
class ImGuiLayer {
public:
    ImGuiLayer(VulkanContext& context, Window& window);
    ~ImGuiLayer();

    void beginFrame();
    void endFrame(vk::CommandBuffer commandBuffer);
    
    /** @brief Gère les événements SDL3 pour ImGui. */
    void onEvent(const union SDL_Event& event);

    /** @brief Indique si ImGui veut capturer la souris (pour bloquer les inputs du jeu). */
    bool wantCaptureMouse() const;
    /** @brief Indique si ImGui veut capturer le clavier. */
    bool wantCaptureKeyboard() const;

    /** @brief Ajoute une texture au système ImGui et retourne son ID. */
    ImTextureID addTexture(vk::Sampler sampler, vk::ImageView view, vk::ImageLayout layout);

private:
    void initFonts();

    VulkanContext& m_context;
    Window& m_window;
    vk::DescriptorPool m_descriptorPool;
};

} // namespace bb3d
