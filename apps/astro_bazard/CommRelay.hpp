#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace astrobazard {

struct CommRelayComponent {
    std::string name = "Relais Inconnu";
    bool isActive = true;

    void serialize(nlohmann::json& j) const {
        j["name"] = name;
        j["isActive"] = isActive;
    }

    void deserialize(const nlohmann::json& j) {
        if (j.contains("name")) name = j["name"];
        if (j.contains("isActive")) isActive = j["isActive"];
    }
};

} // namespace astrobazard
