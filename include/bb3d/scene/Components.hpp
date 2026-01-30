#pragma once

#include "bb3d/core/Base.hpp"
#include "bb3d/render/Model.hpp"
#include "bb3d/core/JsonSerializers.hpp" // GLM JSON serialization
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <string>

#include "bb3d/scene/Camera.hpp"
#include <variant>
#include <functional>
#include <nlohmann/json.hpp>

namespace bb3d {

class Entity; // Forward declaration

using json = nlohmann::json;

/** @brief Composant pour identifier une entité par un nom. */
struct TagComponent {
    std::string tag;

    TagComponent() = default;
    TagComponent(const std::string& t) : tag(t) {}

    void serialize(json& j) const {
        j["tag"] = tag;
    }

    void deserialize(const json& j) {
        if (j.contains("tag")) j.at("tag").get_to(tag);
    }
};

/** @brief Composant gérant la position, rotation et l'échelle. */
struct TransformComponent {
    glm::vec3 translation = { 0.0f, 0.0f, 0.0f };
    glm::vec3 rotation = { 0.0f, 0.0f, 0.0f }; ///< Angles d'Euler.
    glm::vec3 scale = { 1.0f, 1.0f, 1.0f };

    TransformComponent() = default;

    /** @brief Calcule la matrice de transformation 4x4. */
    [[nodiscard]] inline glm::mat4 getTransform() const {
        return glm::translate(glm::mat4(1.0f), translation) * 
               glm::toMat4(glm::quat(rotation)) * 
               glm::scale(glm::mat4(1.0f), scale);
    }

    void serialize(json& j) const {
        j["translation"] = translation;
        j["rotation"] = rotation;
        j["scale"] = scale;
    }

    void deserialize(const json& j) {
        if (j.contains("translation")) j.at("translation").get_to(translation);
        if (j.contains("rotation")) j.at("rotation").get_to(rotation);
        if (j.contains("scale")) j.at("scale").get_to(scale);
    }
};

/** @brief Composant pour l'affichage d'un maillage. */
struct MeshComponent {
    Ref<Mesh> mesh;
    std::string assetPath; // Pour la sérialisation
    glm::vec3 color = {1.0f, 1.0f, 1.0f};

    MeshComponent() = default;
    MeshComponent(Ref<Mesh> m, const std::string& path = "") : mesh(m), assetPath(path) {}

    void serialize(json& j) const {
        j["assetPath"] = assetPath;
        j["color"] = color;
    }

    void deserialize(const json& j) {
        if (j.contains("assetPath")) j.at("assetPath").get_to(assetPath);
        if (j.contains("color")) j.at("color").get_to(color);
        // Note: Le rechargement du mesh via assetPath se fait généralement par le SceneSerializer
    }
};

/** @brief Composant pour l'affichage d'un modèle complet (plusieurs meshes). */
struct ModelComponent {
    Ref<Model> model;
    std::string assetPath; // Pour la sérialisation

    ModelComponent() = default;
    ModelComponent(Ref<Model> m, const std::string& path = "") : model(m), assetPath(path) {}

    void serialize(json& j) const {
        j["assetPath"] = assetPath;
    }

    void deserialize(const json& j) {
        if (j.contains("assetPath")) j.at("assetPath").get_to(assetPath);
    }
};

/** @brief Composant Caméra. */
struct CameraComponent {
    Ref<Camera> camera;
    bool active = true;

    CameraComponent() = default;
    CameraComponent(Ref<Camera> c) : camera(c) {}

    void serialize(json& j) const {
        j["active"] = active;
        // Todo: Sérialiser les propriétés internes de la caméra (FOV, Near, Far, etc.)
        // Cela nécessiterait que la classe Camera elle-même soit sérialisable.
    }

    void deserialize(const json& j) {
        if (j.contains("active")) j.at("active").get_to(active);
    }
};

/** @brief Composant pour attacher un comportement (script C++) à une entité. */
struct NativeScriptComponent {
    std::function<void(Entity, float)> onUpdate;

    NativeScriptComponent() = default;
    
    template<typename Func>
    NativeScriptComponent(Func&& func) : onUpdate(std::forward<Func>(func)) {}

    // Les scripts natifs (pointeurs de fonction) ne sont pas sérialisables par défaut
    void serialize(json&) const {} 
    void deserialize(const json&) {}
};

} // namespace bb3d