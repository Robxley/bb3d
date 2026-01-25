#pragma once

#include "bb3d/core/Core.hpp"
#include "bb3d/core/Config.hpp"

// Forward declare SDL types to keep SDL headers out of the public API
struct SDL_Window;
union SDL_Event;

namespace bb3d {

/**
 * @brief Abstraction de la fenêtre système via SDL3.
 * 
 * Gère la création de la fenêtre native, le contexte Vulkan de surface 
 * et la boucle d'événements OS.
 */
class Window
{
public:
    /**
     * @brief Crée une fenêtre selon la configuration.
     * @param config Configuration contenant le titre, les dimensions, etc.
     */
    explicit Window(const EngineConfig& config);
    
    /**
     * @brief Détruit la fenêtre et quitte SDL.
     */
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    /**
     * @brief Traite les événements OS (Input, Resize, Close).
     */
    void PollEvents();
    
    /**
     * @brief Placeholder pour le swap de buffers (géré par Vulkan SwapChain).
     */
    void SwapBuffers();

    /** @brief Indique si la fenêtre doit se fermer. */
    [[nodiscard]] inline bool ShouldClose() const { return m_ShouldClose; }
    
    /** @brief Récupère le pointeur natif SDL_Window. */
    [[nodiscard]] inline SDL_Window* GetNativeWindow() const { return m_Window; }

private:
    /** @brief Gestion interne des événements SDL. */
    void HandleEvent(SDL_Event& event);

    SDL_Window* m_Window = nullptr;
    bool m_ShouldClose = false;
};

} // namespace bb3d