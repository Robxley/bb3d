#include "bb3d/core/Log.hpp"
#include "bb3d/core/Config.hpp"
#include "bb3d/core/Window.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/render/SwapChain.hpp"
#include "bb3d/render/Shader.hpp"
#include "bb3d/render/GraphicsPipeline.hpp"

#include <SDL3/SDL.h>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace {

struct MockHeader {
    uint32_t headerSize = 32;
    uint32_t headerVersion = 1;
    uint32_t vendorID = 0;
    uint32_t deviceID = 0;
    uint8_t pipelineCacheUUID[VK_UUID_SIZE] = {};
};

} // namespace

int main() {
    bb3d::EngineConfig logConfig;
    logConfig.system.logDirectory = "unit_test_logs";
    logConfig.system.logFileName = "unit_test_35_pipeline_cache.log";
    bb3d::Log::Init(logConfig);
    BB_CORE_INFO("--- Unit Test 35: Persistent Vulkan Pipeline Cache ---");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        BB_CORE_ERROR("SDL initialization failed: {}", SDL_GetError());
        return -1;
    }

    const std::filesystem::path testCachePath = "assets/cache/unit_test_pipeline_cache.bin";
    std::filesystem::path tempTestPath = testCachePath;
    tempTestPath += ".tmp";

    // Clean up any stale test cache file from prior runs
    std::error_code ec;
    std::filesystem::remove(testCachePath, ec);
    std::filesystem::remove(tempTestPath, ec);

    try {
        bb3d::EngineConfig config;
        config.window.title = "Unit Test Pipeline Cache";
        config.window.width = 640;
        config.window.height = 480;
        config.graphics.enablePipelineCache = true;
        config.graphics.pipelineCachePath = testCachePath.string();

        bb3d::Window window(config);

        bb3d::VulkanContext context;
#ifdef NDEBUG
        bool validation = false;
#else
        bool validation = true;
#endif
        context.setPipelineCachePath(testCachePath.string());
        context.init(window.GetNativeWindow(), "Unit Test Pipeline Cache", validation);

        // --- Phase 1: Header Validation Unit Tests ---
        BB_CORE_INFO("Phase 1: Testing PipelineCache header validation...");

        // Case 1: Empty span
        assert(!context.isPipelineCacheValid(std::span<const uint8_t>{}));
        BB_CORE_INFO(" - Case 1 (Empty buffer): Rejected as expected.");

        // Case 2: Buffer smaller than 32 bytes
        std::vector<uint8_t> shortBuffer(16, 0);
        assert(!context.isPipelineCacheValid(shortBuffer));
        BB_CORE_INFO(" - Case 2 (Short buffer < 32 bytes): Rejected as expected.");

        // Case 3: Invalid header size
        MockHeader badSizeHeader;
        badSizeHeader.headerSize = 16; // Invalid
        badSizeHeader.headerVersion = 1;
        assert(!context.isPipelineCacheValid(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(&badSizeHeader), sizeof(badSizeHeader))));
        BB_CORE_INFO(" - Case 3 (Invalid headerSize != 32): Rejected as expected.");

        // Case 4: Invalid header version
        MockHeader badVersionHeader;
        badVersionHeader.headerSize = 32;
        badVersionHeader.headerVersion = 2; // Not VK_PIPELINE_CACHE_HEADER_VERSION_ONE
        assert(!context.isPipelineCacheValid(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(&badVersionHeader), sizeof(badVersionHeader))));
        BB_CORE_INFO(" - Case 4 (Invalid headerVersion != 1): Rejected as expected.");

        // Case 5: Wrong vendor ID
        MockHeader wrongVendorHeader;
        wrongVendorHeader.headerSize = 32;
        wrongVendorHeader.headerVersion = 1;
        wrongVendorHeader.vendorID = 0xDEADBEEF;
        assert(!context.isPipelineCacheValid(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(&wrongVendorHeader), sizeof(wrongVendorHeader))));
        BB_CORE_INFO(" - Case 5 (Wrong vendorID): Rejected as expected.");

        // Case 6: Live cache data from physical device
        std::vector<uint8_t> liveCacheData = context.getDevice().getPipelineCacheData(context.getPipelineCache());
        assert(context.isPipelineCacheValid(liveCacheData));
        BB_CORE_INFO(" - Case 6 (Live GPU Cache Data): Validated successfully (size: {} bytes).", liveCacheData.size());

        // --- Phase 2: Pipeline Creation & Cache Persistence ---
        BB_CORE_INFO("Phase 2: Creating graphics pipeline and saving cache to disk...");
        {
            bb3d::SwapChain swapChain(context, config.window.width, config.window.height);
            bb3d::Shader vertShader(context, "assets/shaders/triangle.vert.spv");
            bb3d::Shader fragShader(context, "assets/shaders/triangle.frag.spv");
            bb3d::GraphicsPipeline pipeline(context, swapChain, vertShader, fragShader, config, {}, {}, false);

            bool saveOk = context.savePipelineCache(testCachePath);
            assert(saveOk);
            assert(std::filesystem::exists(testCachePath));
            auto fileSize = std::filesystem::file_size(testCachePath);
            assert(fileSize >= 32);
            BB_CORE_INFO(" - Cache saved successfully to '{}' (file size: {} bytes).", testCachePath.string(), fileSize);

            // Ensure temporary file was removed / renamed
            assert(!std::filesystem::exists(tempTestPath));

            // --- Phase 3: Load Cache Verification ---
            BB_CORE_INFO("Phase 3: Testing loadPipelineCache on existing and non-existing paths...");
            bool loadOk = context.loadPipelineCache(testCachePath);
            assert(loadOk);

            bool loadMissing = context.loadPipelineCache("assets/cache/non_existent_cache_file.bin");
            assert(!loadMissing);
            BB_CORE_INFO(" - Cache reload validated, missing file handled gracefully.");
        }

        // Clean up first context
        context.cleanup();

        // --- Phase 4: Second Context Initialization with Warm Cache ---
        BB_CORE_INFO("Phase 4: Initializing second VulkanContext using warm persistent cache...");
        {
            bb3d::VulkanContext context2;
            context2.setPipelineCachePath(testCachePath.string());
            context2.init(window.GetNativeWindow(), "Unit Test Warm Cache", validation);

            assert(context2.getPipelineCache());

            // Compile graphics pipeline using warmed cache
            {
                bb3d::SwapChain swapChain2(context2, config.window.width, config.window.height);
                bb3d::Shader vertShader2(context2, "assets/shaders/triangle.vert.spv");
                bb3d::Shader fragShader2(context2, "assets/shaders/triangle.frag.spv");
                bb3d::GraphicsPipeline pipeline2(context2, swapChain2, vertShader2, fragShader2, config, {}, {}, false);

                BB_CORE_INFO(" - GraphicsPipeline created successfully with warmed cache!");
            }
        }

        // Clean up test file
        std::filesystem::remove(testCachePath, ec);
        BB_CORE_INFO("Cleaned up temporary test cache file.");

    } catch (const std::exception& e) {
        BB_CORE_ERROR("Exception during unit test 35: {}", e.what());
        std::filesystem::remove(testCachePath, ec);
        SDL_Quit();
        return -1;
    }

    SDL_Quit();
    BB_CORE_INFO("--- Unit Test 35: SUCCESS ---");
    return 0;
}
