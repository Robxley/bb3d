#pragma once

#include "bb3d/core/Base.hpp"
#include "bb3d/scene/Scene.hpp"
#include <string_view>
#include <nlohmann/json.hpp>

namespace bb3d {

    /**
     * @brief Manages the serialization and deserialization of a complete scene in JSON format.
     */
    class SceneSerializer {
    public:
        SceneSerializer(Ref<Scene> scene);

        /** @brief Saves the scene to a file. */
        void serialize(std::string_view filepath);
        
        /** @brief Loads a scene from a file. */
        bool deserialize(std::string_view filepath);

    private:
        Ref<Scene> m_scene;
    };

} // namespace bb3d
