#pragma once

#include "bb3d/scene/Entity.hpp"
#include <nlohmann/json.hpp>
#include "bb3d/core/Log.hpp"
#include <unordered_map>
#include <string>
#include <functional>

namespace bb3d {

    using SerializeCallback = std::function<void(Entity, nlohmann::json&)>;
    using DeserializeCallback = std::function<void(Entity, const nlohmann::json&)>;

    struct CustomComponentHandlers {
        SerializeCallback serialize;
        DeserializeCallback deserialize;
    };

    using ScriptFunc = std::function<void(Entity, float)>;

    class ComponentRegistry {
    public:
        template<typename T>
        static void Register(const std::string& name) {
            BB_CORE_INFO("ComponentRegistry: Registered custom component '{}'", name);
            s_handlers[name] = {
                [name](Entity entity, nlohmann::json& j) {
                    if (entity.has<T>()) {
                        nlohmann::json c;
                        entity.get<T>().serialize(c);
                        j[name] = c;
                    }
                },
                [name](Entity entity, const nlohmann::json& j) {
                    if (j.contains(name)) {
                        entity.add<T>().get<T>().deserialize(j[name]);
                    }
                }
            };
        }

        /** @brief Map of script names to their logic functions. */
        static void RegisterScript(const std::string& name, ScriptFunc func) {
            BB_CORE_INFO("ComponentRegistry: Registered script '{}'", name);
            s_scripts[name] = func;
        }

        static ScriptFunc GetScript(const std::string& name) {
            if (s_scripts.contains(name)) {
                BB_CORE_DEBUG("ComponentRegistry: Script '{}' found and bound.", name);
                return s_scripts[name];
            }
            
            if (!name.empty()) {
                BB_CORE_WARN("ComponentRegistry: Script '{}' not found in registry!", name);
            }
            return nullptr;
        }

        static const std::unordered_map<std::string, CustomComponentHandlers>& GetHandlers() {
            return s_handlers;
        }

    private:
        inline static std::unordered_map<std::string, CustomComponentHandlers> s_handlers;
        inline static std::unordered_map<std::string, ScriptFunc> s_scripts;
    };
}
