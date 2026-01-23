#include "bb3d/core/Log.hpp"
#include "bb3d/core/Config.hpp"
#include "bb3d/core/Window.hpp"
#include "bb3d/input/InputManager.hpp"

#include <iostream>

void runWindoTest()
{
    BB_PROFILE_FRAME("MainThread");
    BB_PROFILE_SCOPE("Window Test");

    bb3d::Log::Init();

    // Load configuration
    auto config = bb3d::Config::Load("engine_config.json");

    // Create a window
    auto window = bb3d::CreateScope<bb3d::Window>(config);
    
    // Input System
    bb3d::InputManager input;
    input.mapAction("Jump", bb3d::Key::Space);

    BB_INFO("Window test started.");
    BB_INFO("Controls: [ESC] Quit, [SPACE] Jump Action, [W/A/S/D] Move Logs");

    // Main loop
    while (!window->ShouldClose())
    {
        BB_PROFILE_SCOPE("Main Loop");
        window->PollEvents();

        // --- Input Test ---
        
        // Low Level Polling
        if (input.isKeyPressed(bb3d::Key::Escape)) {
            BB_INFO("Escape pressed. Exiting test.");
            break;
        }

        // High Level Action
        static bool jumpPressed = false;
        if (input.isActionPressed("Jump")) {
            if (!jumpPressed) {
                BB_INFO("Action 'Jump' triggered!");
                jumpPressed = true;
            }
        } else {
            jumpPressed = false;
        }

        // Directional (Just logging once to avoid spam)
        static bool wPressed = false;
        if (input.isKeyPressed(bb3d::Key::W)) {
            if (!wPressed) { BB_INFO("Moving Forward (W)"); wPressed = true; }
        } else { wPressed = false; }
    }

    BB_INFO("Window test finished.");
}

int main()
{
    try
    {
        runWindoTest();
    }
    catch (const std::exception& e)
    {
        // Use std::cerr as the logger might not be available
        std::cerr << "A critical error occurred: " << e.what() << std::endl;
        #ifdef _WIN32
            __debugbreak();
        #endif
        return 1;
    }

    return 0;
}
