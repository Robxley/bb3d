#pragma once

#include "bb3d/core/Base.hpp"
#include "bb3d/render/Model.hpp"
#include "bb3d/render/Texture.hpp"
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

// --- Enums ---
enum class BodyType { Static, Dynamic, Kinematic, Character };
enum class LightType { Directional, Point, Spot };

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

    [[nodiscard]] inline glm::vec3 getForward() const { return glm::rotate(glm::quat(rotation), glm::vec3(0.0f, 0.0f, -1.0f)); }
    [[nodiscard]] inline glm::vec3 getUp() const { return glm::rotate(glm::quat(rotation), glm::vec3(0.0f, 1.0f, 0.0f)); }
    [[nodiscard]] inline glm::vec3 getRight() const { return glm::rotate(glm::quat(rotation), glm::vec3(1.0f, 0.0f, 0.0f)); }

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
    }

    void deserialize(const json& j) {
        if (j.contains("active")) j.at("active").get_to(active);
    }
};

/** @brief Composant Lumière. */
struct LightComponent {
    LightType type = LightType::Point;
    glm::vec3 color = { 1.0f, 1.0f, 1.0f };
    float intensity = 1.0f;
    float range = 10.0f;
    bool castShadows = false;

    void serialize(json& j) const {
        j["type"] = static_cast<int>(type);
        j["color"] = color;
        j["intensity"] = intensity;
        j["range"] = range;
        j["castShadows"] = castShadows;
    }

    void deserialize(const json& j) {
        if (j.contains("type")) type = static_cast<LightType>(j.at("type").get<int>());
        if (j.contains("color")) j.at("color").get_to(color);
        if (j.contains("intensity")) j.at("intensity").get_to(intensity);
        if (j.contains("range")) j.at("range").get_to(range);
        if (j.contains("castShadows")) j.at("castShadows").get_to(castShadows);
    }
};

/** @brief Composant Physique RigidBody. */
struct RigidBodyComponent {
    BodyType type = BodyType::Static;
    float mass = 1.0f;
    float friction = 0.5f;
    float restitution = 0.5f;

    void serialize(json& j) const {
        j["type"] = static_cast<int>(type);
        j["mass"] = mass;
        j["friction"] = friction;
        j["restitution"] = restitution;
    }

    void deserialize(const json& j) {
        if (j.contains("type")) type = static_cast<BodyType>(j.at("type").get<int>());
        if (j.contains("mass")) j.at("mass").get_to(mass);
        if (j.contains("friction")) j.at("friction").get_to(friction);
        if (j.contains("restitution")) j.at("restitution").get_to(restitution);
    }
};

struct BoxColliderComponent {
    glm::vec3 halfExtents = { 0.5f, 0.5f, 0.5f };
    void serialize(json& j) const { j["halfExtents"] = halfExtents; }
    void deserialize(const json& j) { if (j.contains("halfExtents")) j.at("halfExtents").get_to(halfExtents); }
};

struct SphereColliderComponent {
    float radius = 0.5f;
    void serialize(json& j) const { j["radius"] = radius; }
    void deserialize(const json& j) { if (j.contains("radius")) j.at("radius").get_to(radius); }
};

struct CapsuleColliderComponent {
    float radius = 0.5f;
    float height = 1.0f;
    void serialize(json& j) const { j["radius"] = radius; j["height"] = height; }
    void deserialize(const json& j) { 
        if (j.contains("radius")) j.at("radius").get_to(radius);
        if (j.contains("height")) j.at("height").get_to(height);
    }
};

/** @brief Composant Audio Source. */
struct AudioSourceComponent {
    std::string assetPath;
    float volume = 1.0f;
    float pitch = 1.0f;
    bool loop = false;
    bool spatial = true;

    void serialize(json& j) const {
        j["assetPath"] = assetPath;
        j["volume"] = volume;
        j["pitch"] = pitch;
        j["loop"] = loop;
        j["spatial"] = spatial;
    }

    void deserialize(const json& j) {
        if (j.contains("assetPath")) j.at("assetPath").get_to(assetPath);
        if (j.contains("volume")) j.at("volume").get_to(volume);
        if (j.contains("pitch")) j.at("pitch").get_to(pitch);
        if (j.contains("loop")) j.at("loop").get_to(loop);
        if (j.contains("spatial")) j.at("spatial").get_to(spatial);
    }
};

struct AudioListenerComponent {
    bool active = true;
    void serialize(json& j) const { j["active"] = active; }
    void deserialize(const json& j) { if (j.contains("active")) j.at("active").get_to(active); }
};

/** @brief Composant Terrain. */
struct TerrainComponent {
    std::string heightmapPath;
    glm::vec3 scale = { 100.0f, 10.0f, 100.0f };

    void serialize(json& j) const {
        j["heightmapPath"] = heightmapPath;
        j["scale"] = scale;
    }

    void deserialize(const json& j) {
        if (j.contains("heightmapPath")) j.at("heightmapPath").get_to(heightmapPath);
        if (j.contains("scale")) j.at("scale").get_to(scale);
    }
};

/** @brief Composant Système de Particules. */
struct ParticleSystemComponent {
    std::string texturePath;
    int maxParticles = 1000;

    void serialize(json& j) const {
        j["texturePath"] = texturePath;
        j["maxParticles"] = maxParticles;
    }

    void deserialize(const json& j) {
        if (j.contains("texturePath")) j.at("texturePath").get_to(texturePath);
        if (j.contains("maxParticles")) j.at("maxParticles").get_to(maxParticles);
    }
};

/** @brief Composant Skybox (Cubemap). */
struct SkyboxComponent {
    Ref<Texture> cubemap;
    void serialize(json&) const {}
    void deserialize(const json&) {}
};

/** @brief Composant SkySphere (Texture 2D équirectangulaire). */
struct SkySphereComponent {
    Ref<Texture> texture;
    void serialize(json&) const {}
    void deserialize(const json&) {}
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