#include "bb3d/core/Config.hpp"
#include "bb3d/core/Log.hpp"
#include <fstream>

namespace bb3d {

    EngineConfig Config::Load(std::string_view path) {
        std::ifstream f((std::string(path)));
        if (!f.is_open()) {
            BB_CORE_WARN("Could not open config file: {}. Using defaults.", path);
            return EngineConfig{};
        }

        try {
            nlohmann::json data = nlohmann::json::parse(f);
            auto config = data.get<EngineConfig>();
            BB_CORE_INFO("Config: Loaded from {0} ({1}x{2} '{3}')", path, config.width, config.height, config.title);
            return config;
        } catch (const std::exception& e) {
            BB_CORE_ERROR("JSON Parsing Error in {}: {}", path, e.what());
            return EngineConfig{};
        }
    }

} // namespace bb3d