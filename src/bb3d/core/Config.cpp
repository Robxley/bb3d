#include "bb3d/core/Config.hpp"
// NOTE: Do NOT include Log.hpp here. Config::Load() is called BEFORE Log::Init(),
// so using BB_CORE_* macros would dereference an uninitialized spdlog logger (null pointer crash).
// Use fprintf(stderr, ...) which is always safe.
#include <fstream>
#include <iomanip>
#include <cstdio>

namespace bb3d {

    EngineConfig Config::Load(std::string_view path) {
        std::ifstream f((std::string(path)));
        if (!f.is_open()) {
            fprintf(stderr, "[Config] Could not open config file: %s. Using defaults.\n", path.data());
            return EngineConfig{};
        }

        try {
            nlohmann::json data = nlohmann::json::parse(f);
            auto config = data.get<EngineConfig>();
            fprintf(stderr, "[Config] Loaded from %s (%dx%d '%s')\n",
                path.data(), config.window.width, config.window.height, config.window.title.c_str());
            return config;
        } catch (const std::exception& e) {
            fprintf(stderr, "[Config] JSON Parsing Error in %s: %s — Using defaults.\n", path.data(), e.what());
            return EngineConfig{};
        }
    }

    void Config::Save(std::string_view path, const EngineConfig& config) {
        std::ofstream f((std::string(path)));
        if (!f.is_open()) {
            fprintf(stderr, "[Config] Could not open file for writing: %s\n", path.data());
            return;
        }

        try {
            nlohmann::json data = config;
            f << std::setw(4) << data << std::endl;
            fprintf(stderr, "[Config] Saved to %s\n", path.data());
        } catch (const std::exception& e) {
            fprintf(stderr, "[Config] JSON Serialization Error for %s: %s\n", path.data(), e.what());
        }
    }

} // namespace bb3d
