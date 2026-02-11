#pragma once

#include "bb3d/core/Base.hpp"
#include "bb3d/scene/Entity.hpp"
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
 * Elle est conçue pour être optionnelle et peut être désactivée en Release via BB3D_ENABLE_EDITOR.
 */
class ImGuiLayer {
public:
    /**
     * @brief Initialise le contexte ImGui pour SDL3 et Vulkan.
     * @param context Contexte Vulkan pour les ressources graphiques.
     * @param window Fenêtre SDL3 pour les événements et le lien natif.
     */
    ImGuiLayer(VulkanContext& context, Window& window);
    
    /** @brief Libère les ressources ImGui (Descriptor Pool, Contexte). */
    ~ImGuiLayer();

    /** @brief Prépare une nouvelle frame ImGui. */
    void beginFrame();
    
    /** 
     * @brief Termine la frame et enregistre les commandes de dessin ImGui.
     * @param commandBuffer Buffer de commande Vulkan actif.
     */
    void endFrame(vk::CommandBuffer commandBuffer);
    
    /** @brief Transmet un événement SDL3 à ImGui. */
    void onEvent(const union SDL_Event& event);

    /** @brief Indique si ImGui capture la souris. */
    [[nodiscard]] bool wantCaptureMouse() const;
    
    /** @brief Indique si ImGui capture le clavier. */
    [[nodiscard]] bool wantCaptureKeyboard() const;

    /** 
     * @brief Enregistre une texture Vulkan pour l'utiliser comme ImTextureID.
     * @return Identifiant utilisable avec ImGui::Image().
     */
    ImTextureID addTexture(vk::Sampler sampler, vk::ImageView view, vk::ImageLayout layout);

    /** @name Accesseurs de Polices
     * @{
     */
    ImFont* getFontRoboto() { return m_fontRoboto; }
    ImFont* getFontAwesome() { return m_fontAwesome; }
    ImFont* getFontSegoe() { return m_fontSegoe; }
    ImFont* getFontNoto() { return m_fontNoto; }
    /** @} */

    /** @brief Affiche la barre de menu principale (Fichier, Édition, etc.). */
    void showMainMenu();

    /** @brief Affiche la hiérarchie de la scène. */
    void showSceneHierarchy(class Scene& scene);
    
    /** @brief Affiche le panneau de configuration de la scène (vue, debug, etc.). */
    void showSceneSettings(class Scene& scene);
    
    /** @brief Affiche l'inspecteur de l'entité sélectionnée. */
    void showInspector();

    /** @brief Affiche la barre d'outils (Play/Pause, Reset). */
    void showToolbar();

    /** @name Gestion de la Sélection
     * @{
     */
    [[nodiscard]] Entity getSelectedEntity() const { return m_selectedEntity; }
    void setSelectedEntity(Entity entity) { m_selectedEntity = entity; }
    /** @} */

    bool isViewportFocused() const { return m_viewportFocused; }
    bool isViewportHovered() const { return m_viewportHovered; }
    void setViewportState(bool focused, bool hovered) { m_viewportFocused = focused; m_viewportHovered = hovered; }

private:
    /** @brief Charge et fusionne les polices (Roboto, FontAwesome, Emojis). */
    void initFonts();
    
    /** @brief Dessine récursivement (bientôt) un noeud d'entité. */
    void drawEntityNode(Entity entity);

    VulkanContext& m_context;
    Window& m_window;
    vk::DescriptorPool m_descriptorPool;

    ImFont *m_fontRoboto = nullptr, *m_fontAwesome = nullptr, *m_fontSegoe = nullptr, *m_fontNoto = nullptr;
    
    Entity m_selectedEntity;
    bool m_viewportFocused = false;
    bool m_viewportHovered = false;
};

} // namespace bb3d