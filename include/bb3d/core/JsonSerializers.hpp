#pragma once

#include <nlohmann/json.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Extension du namespace glm pour la sérialisation JSON via ADL
namespace glm {

    // vec2
    inline void to_json(nlohmann::json& j, const vec2& v) {
        j = nlohmann::json{{"x", v.x}, {"y", v.y}};
    }

    inline void from_json(const nlohmann::json& j, vec2& v) {
        if (j.contains("x")) j.at("x").get_to(v.x);
        if (j.contains("y")) j.at("y").get_to(v.y);
    }

    // vec3
    inline void to_json(nlohmann::json& j, const vec3& v) {
        j = nlohmann::json{{"x", v.x}, {"y", v.y}, {"z", v.z}};
    }

    inline void from_json(const nlohmann::json& j, vec3& v) {
        if (j.contains("x")) j.at("x").get_to(v.x);
        if (j.contains("y")) j.at("y").get_to(v.y);
        if (j.contains("z")) j.at("z").get_to(v.z);
    }

    // vec4
    inline void to_json(nlohmann::json& j, const vec4& v) {
        j = nlohmann::json{{"x", v.x}, {"y", v.y}, {"z", v.z}, {"w", v.w}};
    }

    inline void from_json(const nlohmann::json& j, vec4& v) {
        if (j.contains("x")) j.at("x").get_to(v.x);
        if (j.contains("y")) j.at("y").get_to(v.y);
        if (j.contains("z")) j.at("z").get_to(v.z);
        if (j.contains("w")) j.at("w").get_to(v.w);
    }

    // quat
    inline void to_json(nlohmann::json& j, const quat& q) {
        j = nlohmann::json{{"x", q.x}, {"y", q.y}, {"z", q.z}, {"w", q.w}};
    }

    inline void from_json(const nlohmann::json& j, quat& q) {
        if (j.contains("x")) j.at("x").get_to(q.x);
        if (j.contains("y")) j.at("y").get_to(q.y);
        if (j.contains("z")) j.at("z").get_to(q.z);
        if (j.contains("w")) j.at("w").get_to(q.w);
    }

    // mat4 (sérialisé comme un tableau de 16 flottants)
    inline void to_json(nlohmann::json& j, const mat4& m) {
        j = nlohmann::json::array();
        const float* p = (const float*)&m;
        for (int i = 0; i < 16; ++i) j.push_back(p[i]);
    }

    inline void from_json(const nlohmann::json& j, mat4& m) {
        if (j.is_array() && j.size() == 16) {
            float* p = (float*)&m;
            for (int i = 0; i < 16; ++i) j.at(i).get_to(p[i]);
        }
    }
}
