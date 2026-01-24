#pragma once

#include <string>
#include <string_view>
#include <nlohmann/json.hpp>

namespace bb3d {

    struct EngineConfig {
        std::string title = "biobazard3d";
        int width = 1280;
        int height = 720;
        bool vsync = true;
        int fpsMax = 0;
        
        // Rasterization defaults
        std::string cullMode = "Back"; // None, Front, Back, FrontAndBack
        std::string frontFace = "CCW"; // CW, CCW

        bool enablePhysics = true;
        std::string physicsBackend = "jolt";
        bool enableAudio = true;
        bool enableJobSystem = true;
        int maxThreads = 8;
        std::string assetPath = "assets";

        // Macro nlohmann_json pour la sérialisation automatique
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(EngineConfig, title, width, height, vsync, fpsMax, cullMode, frontFace, enablePhysics, physicsBackend, enableAudio, enableJobSystem, maxThreads, assetPath)
    };

    class Config {
    public:
        static EngineConfig Load(std::string_view path);
    };

} // namespace bb3d