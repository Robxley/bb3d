#pragma once

#include "bb3d/scene/Entity.hpp"
#include <nlohmann/json.hpp>
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

    class ComponentRegistry {
    public:
        template<typename T>
        static void Register(const std::string& name) {
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

        static const std::unordered_map<std::string, CustomComponentHandlers>& GetHandlers() {
            return s_handlers;
        }

    private:
        inline static std::unordered_map<std::string, CustomComponentHandlers> s_handlers;
    };
}
