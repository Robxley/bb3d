#include "bb3d/core/Config.hpp"
#include "bb3d/core/Log.hpp"
#include <fstream>
#include <iomanip>

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
            BB_CORE_INFO("Config: Loaded from {0} ({1}x{2} '{3}')", 
                path, config.window.width, config.window.height, config.window.title);
            return config;
        } catch (const std::exception& e) {
            BB_CORE_ERROR("JSON Parsing Error in {}: {}", path, e.what());
            BB_CORE_WARN("Using default configuration due to error.");
            return EngineConfig{};
        }
    }

    void Config::Save(std::string_view path, const EngineConfig& config) {
        std::ofstream f((std::string(path)));
        if (!f.is_open()) {
            BB_CORE_ERROR("Could not open file for writing: {}", path);
            return;
        }

        try {
            nlohmann::json data = config;
            f << std::setw(4) << data << std::endl;
            BB_CORE_INFO("Config: Saved to {0}", path);
        } catch (const std::exception& e) {
            BB_CORE_ERROR("JSON Serialization Error for {}: {}", path, e.what());
        }
    }

} // namespace bb3d
