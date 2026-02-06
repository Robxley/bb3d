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
#include <atomic>

namespace JPH { class BodyID; }

namespace bb3d {

class Entity; // Forward declaration

using json = nlohmann::json;

// --- Enums ---
enum class BodyType { Static, Dynamic, Kinematic, Character };
enum class LightType { Directional, Point, Spot };

/** @brief Identifiant unique déterministe et thread-safe pour les entités. */
struct IDComponent {
    uint64_t id;

    // Compteur global atomique pour garantir l'unicité et le thread-safety
    static inline std::atomic<uint64_t> s_NextID{1}; 

    IDComponent() : id(s_NextID++) {}
    
    /** @brief Constructeur explicite pour le chargement ou la duplication. */
    IDComponent(uint64_t _id) : id(_id) {
        // Si on force un ID (ex: chargement), on s'assure que le compteur est à jour pour éviter les collisions futures
        uint64_t next = s_NextID.load();
        while (_id >= next && !s_NextID.compare_exchange_weak(next, _id + 1));
    }

    void serialize(json& j) const { j["id"] = id; }
    void deserialize(const json& j) { 
        if(j.contains("id")) {
            uint64_t loadedID = j.at("id").get<uint64_t>();
            id = loadedID;
            // Mise à jour du compteur global si nécessaire
            uint64_t next = s_NextID.load();
            while (loadedID >= next && !s_NextID.compare_exchange_weak(next, loadedID + 1));
        }
    }
};

/** @brief Composant pour identifier une entité par un nom (Debug/Editor only). */
struct TagComponent {
    std::string tag;

    TagComponent() = default;
    // Optimisation : Passage par valeur + std::move pour éviter une copie inutile si rvalue
    TagComponent(std::string t) : tag(std::move(t)) {}

    void serialize(json& j) const {
        j["tag"] = tag;
    }

    void deserialize(const json& j) {
        if (j.contains("tag")) j.at("tag").get_to(tag);
    }
};

/** @brief Composant gérant la position, rotation et l'échelle dans l'espace 3D. */
struct TransformComponent {
    glm::vec3 translation = { 0.0f, 0.0f, 0.0f };
    glm::vec3 rotation = { 0.0f, 0.0f, 0.0f }; ///< Angles d'Euler en radians (X:Pitch, Y:Yaw, Z:Roll).
    glm::vec3 scale = { 1.0f, 1.0f, 1.0f };

    TransformComponent() = default;
    TransformComponent(const glm::vec3& t, const glm::vec3& r = {0,0,0}, const glm::vec3& s = {1,1,1}) 
        : translation(t), rotation(r), scale(s) {}

    /** @brief Calcule la matrice de transformation 4x4 (Model Matrix). */
    [[nodiscard]] inline glm::mat4 getTransform() const {
        return glm::translate(glm::mat4(1.0f), translation) * 
               glm::toMat4(glm::quat(rotation)) * 
               glm::scale(glm::mat4(1.0f), scale);
    }

    /** @brief Vecteur Forward (Z-) local. */
    [[nodiscard]] inline glm::vec3 getForward() const { return glm::rotate(glm::quat(rotation), glm::vec3(0.0f, 0.0f, -1.0f)); }
    /** @brief Vecteur Up (Y+) local. */
    [[nodiscard]] inline glm::vec3 getUp() const { return glm::rotate(glm::quat(rotation), glm::vec3(0.0f, 1.0f, 0.0f)); }
    /** @brief Vecteur Right (X+) local. */
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

/** @brief Composant Caméra (Données optiques). */
struct CameraComponent {
    Ref<Camera> camera;
    bool active = true;

    CameraComponent() = default;
    CameraComponent(Ref<Camera> c) : camera(c) {}

    void serialize(json& j) const {
        j["active"] = active;
        if (camera) {
            // On sauvegarde juste les propriétés optiques de base si besoin
            // Pour l'instant, on suppose que le type de contrôleur gère la "logique"
            // et que la CameraComponent est juste le "rendu".
        }
    }

    void deserialize(const json& j) {
        if (j.contains("active")) j.at("active").get_to(active);
        // Note: La création de la caméra (Perspective/Ortho) doit être gérée lors de l'ajout du composant
        // ou via un système de sérialisation plus complet de la classe Camera elle-même.
    }
};

/** @brief Contrôleur pour caméra FPS (Clavier/Souris). */
struct FPSControllerComponent {
    glm::vec3 movementSpeed = {10.0f, 10.0f, 10.0f};
    glm::vec2 rotationSpeed = {0.1f, 0.1f};
    
    // État interne (Yaw/Pitch)
    float yaw = -90.0f;
    float pitch = 0.0f;

    void serialize(json& j) const {
        j["movementSpeed"] = movementSpeed;
        j["rotationSpeed"] = rotationSpeed;
        j["yaw"] = yaw;
        j["pitch"] = pitch;
    }

    void deserialize(const json& j) {
        if (j.contains("movementSpeed")) j.at("movementSpeed").get_to(movementSpeed);
        if (j.contains("rotationSpeed")) j.at("rotationSpeed").get_to(rotationSpeed);
        if (j.contains("yaw")) j.at("yaw").get_to(yaw);
        if (j.contains("pitch")) j.at("pitch").get_to(pitch);
    }
};

/** @brief Contrôleur pour caméra Orbitale. */
struct OrbitControllerComponent {
    glm::vec3 target = {0.0f, 0.0f, 0.0f};
    float distance = 10.0f;
    float minDistance = 1.0f;
    float maxDistance = 100.0f;
    
    glm::vec2 rotationSpeed = {0.2f, 0.2f};
    float zoomSpeed = 2.0f;

    // État interne
    float yaw = 0.0f;
    float pitch = 0.0f;

    void serialize(json& j) const {
        j["target"] = target;
        j["distance"] = distance;
        j["rotationSpeed"] = rotationSpeed;
        j["zoomSpeed"] = zoomSpeed;
        j["yaw"] = yaw;
        j["pitch"] = pitch;
    }

    void deserialize(const json& j) {
        if (j.contains("target")) j.at("target").get_to(target);
        if (j.contains("distance")) j.at("distance").get_to(distance);
        if (j.contains("rotationSpeed")) j.at("rotationSpeed").get_to(rotationSpeed);
        if (j.contains("zoomSpeed")) j.at("zoomSpeed").get_to(zoomSpeed);
        if (j.contains("yaw")) j.at("yaw").get_to(yaw);
        if (j.contains("pitch")) j.at("pitch").get_to(pitch);
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
    glm::vec3 initialLinearVelocity = {0.0f, 0.0f, 0.0f};

    // Runtime Jolt data
    uint32_t bodyID = 0xFFFFFFFF; // JPH::BodyID::mID

    void serialize(json& j) const {
        j["type"] = static_cast<int>(type);
        j["mass"] = mass;
        j["friction"] = friction;
        j["restitution"] = restitution;
        j["initialLinearVelocity"] = initialLinearVelocity;
    }

    void deserialize(const json& j) {
        if (j.contains("type")) type = static_cast<BodyType>(j.at("type").get<int>());
        if (j.contains("mass")) j.at("mass").get_to(mass);
        if (j.contains("friction")) j.at("friction").get_to(friction);
        if (j.contains("restitution")) j.at("restitution").get_to(restitution);
        if (j.contains("initialLinearVelocity")) j.at("initialLinearVelocity").get_to(initialLinearVelocity);
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

struct MeshColliderComponent {
    Ref<Mesh> mesh;
    bool convex = false;
    void serialize(json&) const {}
    void deserialize(const json&) {}
};

struct CharacterControllerComponent {
    float stepHeight = 0.3f;
    float maxSlopeAngle = 45.0f; // Degrees
    
    // State
    glm::vec3 velocity = {0, 0, 0};
    bool isGrounded = false;

    void serialize(json& j) const {
        j["stepHeight"] = stepHeight;
        j["maxSlopeAngle"] = maxSlopeAngle;
    }
    void deserialize(const json& j) {
        if (j.contains("stepHeight")) j.at("stepHeight").get_to(stepHeight);
        if (j.contains("maxSlopeAngle")) j.at("maxSlopeAngle").get_to(maxSlopeAngle);
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
