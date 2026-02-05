#include "bb3d/core/Window.hpp"
#include "bb3d/core/Log.hpp"

#include <SDL3/SDL.h>

namespace bb3d {

Window::Window(const EngineConfig& config)
{
    BB_PROFILE_SCOPE("Window::Window");

    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        BB_CORE_FATAL("Failed to initialize SDL: {}", SDL_GetError());
        throw std::runtime_error("SDL Init failed");
    }

    // Create the window
    // SDL_WINDOW_VULKAN : Indispensable pour créer une surface Vulkan par la suite.
    // SDL_WINDOW_RESIZABLE : Permet à l'utilisateur de redimensionner la fenêtre (génère des événements SDL_EVENT_WINDOW_RESIZED).
    m_Window = SDL_CreateWindow(
        config.window.title.c_str(),
        config.window.width,
        config.window.height,
        SDL_WINDOW_VULKAN | (config.window.resizable ? SDL_WINDOW_RESIZABLE : 0) | (config.window.fullscreen ? SDL_WINDOW_FULLSCREEN : 0)
    );

    if (!m_Window) {
        BB_CORE_FATAL("Failed to create window: {}", SDL_GetError());
        SDL_Quit();
        throw std::runtime_error("SDL_CreateWindow failed");
    }

    BB_CORE_INFO("Window created: '{}' ({}x{})", config.window.title, config.window.width, config.window.height);
}

Window::~Window()
{
    BB_PROFILE_SCOPE("Window::~Window");

    if (m_Window) {
        SDL_DestroyWindow(m_Window);
    }
    SDL_Quit();
    BB_CORE_INFO("Window destroyed, SDL quit.");
}

void Window::PollEvents()
{
    BB_PROFILE_SCOPE("Window::PollEvents");

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        HandleEvent(e);
        if (m_EventCallback) {
            m_EventCallback(e);
        }
    }
}

void Window::SwapBuffers()
{
    // This will be handled by the Vulkan renderer, but we can keep the method signature.
    // In a pure SDL renderer, we would call SDL_GL_SwapWindow or similar.
}

void Window::HandleEvent(SDL_Event& event)
{
    if (event.type == SDL_EVENT_QUIT) {
        BB_CORE_INFO("Window: Close event received (SDL_EVENT_QUIT)");
        m_ShouldClose = true;
    }
    if (event.type == SDL_EVENT_KEY_DOWN) {
        if (event.key.key == SDLK_ESCAPE) {
            BB_CORE_INFO("Window: Close event received (ESC key)");
            m_ShouldClose = true;
        }
    }
}

int Window::GetWidth() const
{
    int w;
    SDL_GetWindowSize(m_Window, &w, nullptr);
    return w;
}

int Window::GetHeight() const
{
    int h;
    SDL_GetWindowSize(m_Window, nullptr, &h);
    return h;
}

} // namespace bb3d
