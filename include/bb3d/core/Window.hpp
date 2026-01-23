#pragma once

#include "bb3d/core/Core.hpp"
#include "bb3d/core/Config.hpp"

// Forward declare SDL types to keep SDL headers out of the public API
struct SDL_Window;
union SDL_Event;

namespace bb3d {

class Window
{
public:
    explicit Window(const EngineConfig& config);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    void PollEvents();
    void SwapBuffers();

    bool ShouldClose() const { return m_ShouldClose; }
    SDL_Window* GetNativeWindow() const { return m_Window; }

private:
    void HandleEvent(SDL_Event& event);

    SDL_Window* m_Window = nullptr;
    bool m_ShouldClose = false;
};

} // namespace bb3d
