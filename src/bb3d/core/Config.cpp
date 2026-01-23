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
            return data.get<EngineConfig>();
        } catch (const std::exception& e) {
            BB_CORE_ERROR("JSON Parsing Error in {}: {}", path, e.what());
            return EngineConfig{};
        }
    }

} // namespace bb3d