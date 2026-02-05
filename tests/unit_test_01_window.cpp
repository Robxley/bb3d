#include "bb3d/core/Log.hpp"
#include "bb3d/core/Config.hpp"
#include "bb3d/core/Window.hpp"
#include "bb3d/input/InputManager.hpp"

#include <iostream>
#include <glm/glm.hpp>

void runWindoTest()
{
    BB_PROFILE_FRAME("MainThread");
    BB_PROFILE_SCOPE("Window Test");

    bb3d::EngineConfig logConfig;
    logConfig.system.logDirectory = "unit_test_logs";
    logConfig.system.logFileName = "unit_test_01.log";
    bb3d::Log::Init(logConfig);

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
        input.update();

        // --- Input Test ---
        
        // Low Level Polling
        if (input.isKeyJustPressed(bb3d::Key::Escape)) {
            BB_INFO("Escape pressed. Exiting test.");
            break;
        }

        // High Level Action
        if (input.isActionJustPressed("Jump")) {
            BB_INFO("Action 'Jump' triggered (Just Pressed)!");
        }

        // Directional
        if (input.isKeyJustPressed(bb3d::Key::W)) {
            BB_INFO("Moving Forward (W) - Just Pressed");
        }
        
        if (input.isKeyJustReleased(bb3d::Key::W)) {
            BB_INFO("Stopped Moving Forward (W) - Just Released");
        }

        // Mouse delta test
        auto delta = input.getMouseDelta();
        if (glm::length(delta) > 50.0f) {
            BB_INFO("Fast mouse movement: {0}, {1}", delta.x, delta.y);
        }
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
