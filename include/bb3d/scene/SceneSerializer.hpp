#pragma once

#include "bb3d/core/Base.hpp"
#include "bb3d/scene/Scene.hpp"
#include <string_view>
#include <nlohmann/json.hpp>

namespace bb3d {

    /**
     * @brief Gère la sérialisation et la désérialisation d'une scène complète au format JSON.
     */
    class SceneSerializer {
    public:
        SceneSerializer(Ref<Scene> scene);

        /** @brief Sauvegarde la scène dans un fichier. */
        void serialize(std::string_view filepath);
        
        /** @brief Charge une scène depuis un fichier. */
        bool deserialize(std::string_view filepath);

    private:
        Ref<Scene> m_scene;
    };

} // namespace bb3d
