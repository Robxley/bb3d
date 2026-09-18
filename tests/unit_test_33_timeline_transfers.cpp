#include "bb3d/core/Log.hpp"
#include "bb3d/core/Config.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/render/Texture.hpp"
#include <SDL3/SDL.h>
#include <vector>
#include <cassert>
#include <iostream>

int main() {
    bb3d::EngineConfig logConfig;
    logConfig.system.logDirectory = "unit_test_logs";
    logConfig.system.logFileName = "unit_test_33_timeline_transfers.log";
    bb3d::Log::Init(logConfig);
    BB_CORE_INFO("--- Unit Test 33: Timeline Semaphores & Asynchronous Transfers (B11, P11) ---");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        BB_CORE_ERROR("SDL initialization failed: {}", SDL_GetError());
        return -1;
    }

    try {
        bb3d::VulkanContext context;

#ifdef NDEBUG
        bool validation = false;
#else
        bool validation = true;
#endif
        context.init(nullptr, "Unit Test Timeline Transfers", validation);

        const auto& features = context.getEnabledFeatures();
        if (!features.timelineSemaphore) {
            BB_CORE_ERROR("Vulkan Timeline Semaphores are not enabled on device!");
            SDL_Quit();
            return -1;
        }

        // --- Test 1: Timeline Semaphore Initial State ---
        BB_CORE_INFO("Testing Timeline Semaphore creation and initial state...");
        vk::Semaphore transferSem = context.getTransferTimelineSemaphore();
        assert(transferSem != vk::Semaphore(nullptr));
        assert(context.getCompletedTransferTimelineValue() == 0);
        assert(context.getTransferTimelineValue() == 0);
        BB_CORE_INFO("Initial timeline value is 0 (as expected).");

        // --- Test 2: Asynchronous Transfers with Monotonic Increments ---
        BB_CORE_INFO("Testing monotonic async transfer submissions...");
        vk::CommandBuffer cb1 = context.beginTransferCommands();
        // Record a pipeline barrier or empty commands
        cb1.setLineWidth(1.0f); // harmless dynamic state or pipeline barrier
        uint64_t val1 = context.endTransferCommandsAsync(cb1);
        assert(val1 == 1);
        assert(context.getTransferTimelineValue() == 1);

        vk::CommandBuffer cb2 = context.beginTransferCommands();
        cb2.setLineWidth(1.0f);
        uint64_t val2 = context.endTransferCommandsAsync(cb2);
        assert(val2 == 2);
        assert(context.getTransferTimelineValue() == 2);

        // Wait for transfer completion via timeline
        context.waitTransferTimeline(val2);
        uint64_t completedVal = context.getCompletedTransferTimelineValue();
        assert(completedVal >= 2);
        BB_CORE_INFO("Completed timeline value reaches {} (>= 2).", completedVal);

        // Poll completions to recycle command buffers
        context.pollTransferCompletions();

        // --- Test 3: Texture Lifecycle with Non-blocking Timeline ---
        BB_CORE_INFO("Testing Texture upload with Timeline Semaphore...");
        std::vector<uint8_t> pixels(32 * 32 * 4, 128);
        auto testTexture = bb3d::CreateRef<bb3d::Texture>(context, std::as_bytes(std::span(pixels)), 32, 32, true);

        // Texture upload was submitted to transfer queue; check readiness
        bool readyImmediate = testTexture->isReady();
        BB_CORE_INFO("Texture isReady() query result: {}", readyImmediate);

        // Wait on the transfer timeline for the texture
        context.waitTransferTimeline(context.getTransferTimelineValue());
        assert(testTexture->isReady() == true);
        assert(testTexture->getImageView() != vk::ImageView(nullptr));
        BB_CORE_INFO("Texture is ready with valid image view and timeline completed.");

        // Cleanup
        testTexture.reset();
        context.cleanup();

        BB_CORE_INFO("Unit Test 33 PASSED!");
    } catch (const std::exception& e) {
        BB_CORE_ERROR("Fatal error in Unit Test 33: {}", e.what());
        SDL_Quit();
        return -1;
    }

    SDL_Quit();
    return 0;
}
