#include "bb3d/core/Log.hpp"
#include "bb3d/core/Config.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/render/DebugUtils.hpp"
#include <SDL3/SDL.h>
#include <cassert>
#include <iostream>

#if defined(BB_PROFILE)
#include <tracy/TracyVulkan.hpp>
#endif

int main() {
    bb3d::EngineConfig logConfig;
    logConfig.system.logDirectory = "unit_test_logs";
    logConfig.system.logFileName = "unit_test_34_debug_utils_profiling.log";
    bb3d::Log::Init(logConfig);
    BB_CORE_INFO("--- Unit Test 34: DebugUtils Instrumentation & Profiling Tracy GPU ---");

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
        context.init(nullptr, "Unit Test DebugUtils & Profiling", validation);

        // --- Test 1: DebugUtils Extension Discovery & Support ---
        BB_CORE_INFO("Testing DebugUtils extension support...");
        bool isSupported = context.isDebugUtilsSupported();
        BB_CORE_INFO("VK_EXT_debug_utils supported on current instance: {}", isSupported ? "YES" : "NO");

        // --- Test 2: Debug Object Naming ---
        BB_CORE_INFO("Testing type-safe object naming via setObjectName...");
        vk::Semaphore transferSem = context.getTransferTimelineSemaphore();
        context.setObjectName(transferSem, "TestTransferTimelineSemaphore");

        vk::CommandPool cmdPool = context.getShortLivedCommandPool();
        context.setObjectName(cmdPool, "TestShortLivedCommandPool");

        // Create a dedicated semaphore for naming and destroy it
        vk::SemaphoreTypeCreateInfo semType(vk::SemaphoreType::eBinary, 0);
        vk::SemaphoreCreateInfo semInfo;
        semInfo.pNext = &semType;
        vk::Semaphore customSem = context.getDevice().createSemaphore(semInfo);
        context.setObjectName(customSem, "TestCustomBinarySemaphore");
        context.getDevice().destroySemaphore(customSem);
        BB_CORE_INFO("Debug object naming completed without error.");

        // --- Test 3: Command Buffer Debug Labeling & RAII ScopedDebugLabel ---
        BB_CORE_INFO("Testing ScopedDebugLabel RAII and manual labeling...");
        vk::CommandBuffer cb = context.beginSingleTimeCommands();

        // Manual begin / insert / end
        context.cmdBeginDebugLabel(cb, "Manual Root Pass", bb3d::DebugColor::ScenePbrPass);
        context.cmdInsertDebugLabel(cb, "Checkpoint Marker", bb3d::DebugColor::ShadowPass);

        // RAII Scoped label
        {
            bb3d::ScopedDebugLabel scopedPass1(context, cb, "RAII Nested Pass 1", bb3d::DebugColor::SkyboxPass);
            {
                bb3d::ScopedDebugLabel scopedPass2(context, cb, "RAII Nested SubPass 2", bb3d::DebugColor::PickingPass);
                // Perform a dummy no-op barrier
                cb.pipelineBarrier2(vk::DependencyInfo{});
            }
        }

        context.cmdEndDebugLabel(cb);
        context.endSingleTimeCommands(cb);
        BB_CORE_INFO("ScopedDebugLabel RAII nesting and command buffer submission validated.");

        // --- Test 4: Tracy GPU Integration (conditional on BB_PROFILE) ---
#if defined(BB_PROFILE)
        BB_CORE_INFO("Testing Tracy GPU context initialization and collection...");
        vk::CommandBuffer tracyInitCb = context.allocateShortLivedCommandBuffer();
        TracyVkCtx tracyCtx = TracyVkContext(
            static_cast<VkPhysicalDevice>(context.getPhysicalDevice()),
            static_cast<VkDevice>(context.getDevice()),
            static_cast<VkQueue>(context.getGraphicsQueue()),
            static_cast<VkCommandBuffer>(tracyInitCb)
        );
        context.freeShortLivedCommandBuffer(tracyInitCb);

        if (tracyCtx != nullptr) {
            BB_CORE_INFO("TracyVkContext created successfully.");
            vk::CommandBuffer profileCb = context.beginSingleTimeCommands();
            {
                TracyVkZone(tracyCtx, static_cast<VkCommandBuffer>(profileCb), "TracyUnitTestZone");
                profileCb.pipelineBarrier2(vk::DependencyInfo{});
            }
            TracyVkCollect(tracyCtx, static_cast<VkCommandBuffer>(profileCb));
            context.endSingleTimeCommands(profileCb);
            TracyVkDestroy(tracyCtx);
            BB_CORE_INFO("Tracy GPU zone and collect executed successfully.");
        } else {
            BB_CORE_WARN("TracyVkContext returned nullptr (TRACY_ENABLE may be disabled or mocked).");
        }
#else
        BB_CORE_INFO("Tracy GPU profiling (BB_PROFILE) not enabled in this build config.");
#endif

    } catch (const std::exception& e) {
        BB_CORE_ERROR("Exception caught during unit test: {}", e.what());
        SDL_Quit();
        return -1;
    }

    SDL_Quit();
    BB_CORE_INFO("--- Unit Test 34: SUCCESS ---");
    return 0;
}
