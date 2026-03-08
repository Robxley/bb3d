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
enum class PrimitiveType { None = 0, Cube, Sphere, Plane };
enum class SimpleAnimationType { Rotation = 0, Translation, Waypoints };

/** @brief Unique deterministic and thread-safe identifier for entities. */
struct IDComponent {
    uint64_t id;

    // Global atomic counter to guarantee uniqueness and thread-safety
    static inline std::atomic<uint64_t> s_NextID{1}; 

    IDComponent() : id(s_NextID++) {}
    
    /** @brief Explicit constructor for loading or duplication. */
    IDComponent(uint64_t _id) : id(_id) {
        // If we force an ID (e.g. loading), we ensure the counter is up to date to avoid future collisions
        uint64_t next = s_NextID.load();
        while (_id >= next && !s_NextID.compare_exchange_weak(next, _id + 1));
    }

    void serialize(json& j) const { j["id"] = id; }
    void deserialize(const json& j) { 
        if(j.contains("id")) {
            uint64_t loadedID = j.at("id").get<uint64_t>();
            id = loadedID;
            // Update global counter if necessary
            uint64_t next = s_NextID.load();
            while (loadedID >= next && !s_NextID.compare_exchange_weak(next, loadedID + 1));
        }
    }
};

/** @brief Component to identify an entity by a name (Debug/Editor only). */
struct TagComponent {
    std::string tag;

    TagComponent() = default;
    // Optimization: Pass by value + std::move to avoid unnecessary copy if rvalue
    TagComponent(std::string t) : tag(std::move(t)) {}

    void serialize(json& j) const {
        j["tag"] = tag;
    }

    void deserialize(const json& j) {
        if (j.contains("tag")) j.at("tag").get_to(tag);
    }
};

/** @brief Component managing position, rotation, and scale in 3D space. */
struct TransformComponent {
    glm::vec3 translation = { 0.0f, 0.0f, 0.0f };
    glm::vec3 rotation = { 0.0f, 0.0f, 0.0f }; ///< Euler angles in radians (X:Pitch, Y:Yaw, Z:Roll).
    glm::vec3 scale = { 1.0f, 1.0f, 1.0f };

    // Initialization state for scene reset
    glm::vec3 initialTranslation = { 0.0f, 0.0f, 0.0f };
    glm::vec3 initialRotation = { 0.0f, 0.0f, 0.0f };
    glm::vec3 initialScale = { 1.0f, 1.0f, 1.0f };

    TransformComponent() = default;
    TransformComponent(const glm::vec3& t, const glm::vec3& r = {0,0,0}, const glm::vec3& s = {1,1,1}) 
        : translation(t), rotation(r), scale(s), initialTranslation(t), initialRotation(r), initialScale(s) {}

    /** @brief Saves current state as initialization state. */
    void saveInitialState() {
        initialTranslation = translation;
        initialRotation = rotation;
        initialScale = scale;
    }

    /** @brief Restores state from initialization backup. */
    void resetToInitial() {
        translation = initialTranslation;
        rotation = initialRotation;
        scale = initialScale;
    }

    /** @brief Calculates the 4x4 transformation matrix (Model Matrix). */
    [[nodiscard]] inline glm::mat4 getTransform() const {
        return glm::translate(glm::mat4(1.0f), translation) * 
               glm::toMat4(glm::quat(rotation)) * 
               glm::scale(glm::mat4(1.0f), scale);
    }

    /** @brief Local Forward vector (Z-). */
    [[nodiscard]] inline glm::vec3 getForward() const { return glm::rotate(glm::quat(rotation), glm::vec3(0.0f, 0.0f, -1.0f)); }
    /** @brief Local Up vector (Y+). */
    [[nodiscard]] inline glm::vec3 getUp() const { return glm::rotate(glm::quat(rotation), glm::vec3(0.0f, 1.0f, 0.0f)); }
    /** @brief Local Right vector (X+). */
    [[nodiscard]] inline glm::vec3 getRight() const { return glm::rotate(glm::quat(rotation), glm::vec3(1.0f, 0.0f, 0.0f)); }

    void serialize(json& j) const {
        j["translation"] = translation;
        j["rotation"] = rotation;
        j["scale"] = scale;
        j["initialTranslation"] = initialTranslation;
        j["initialRotation"] = initialRotation;
        j["initialScale"] = initialScale;
    }

    void deserialize(const json& j) {
        if (j.contains("translation")) j.at("translation").get_to(translation);
        if (j.contains("rotation")) j.at("rotation").get_to(rotation);
        if (j.contains("scale")) j.at("scale").get_to(scale);
        if (j.contains("initialTranslation")) j.at("initialTranslation").get_to(initialTranslation);
        else initialTranslation = translation;
        if (j.contains("initialRotation")) j.at("initialRotation").get_to(initialRotation);
        else initialRotation = rotation;
        if (j.contains("initialScale")) j.at("initialScale").get_to(initialScale);
        else initialScale = scale;
    }
};

/** @brief Component for displaying a mesh. */
struct MeshComponent {
    Ref<Mesh> mesh;
    std::string assetPath; ///< Path used for serialization and hot-reloading.
    PrimitiveType primitiveType = PrimitiveType::None;
    glm::vec3 color = {1.0f, 1.0f, 1.0f};
    bool visible = true; ///< If false, the mesh is skipped during the rendering pass.

    MeshComponent() = default;
    MeshComponent(Ref<Mesh> m, const std::string& path = "", PrimitiveType type = PrimitiveType::None) : mesh(m), assetPath(path), primitiveType(type) {}

    void serialize(json& j) const {
        j["assetPath"] = assetPath;
        j["primitiveType"] = static_cast<int>(primitiveType);
        j["color"] = color;
        j["visible"] = visible;
    }

    void deserialize(const json& j) {
        if (j.contains("assetPath")) j.at("assetPath").get_to(assetPath);
        if (j.contains("primitiveType")) primitiveType = static_cast<PrimitiveType>(j.at("primitiveType").get<int>());
        if (j.contains("color")) j.at("color").get_to(color);
        if (j.contains("visible")) j.at("visible").get_to(visible);
    }
};

/** @brief Component for displaying a complete model (multiple meshes). */
struct ModelComponent {
    Ref<Model> model;
    std::string assetPath; ///< Path used for serialization.
    bool visible = true; ///< If false, all meshes in this model are skipped during rendering.

    ModelComponent() = default;
    ModelComponent(Ref<Model> m, const std::string& path = "") : model(m), assetPath(path) {}

    void serialize(json& j) const {
        j["assetPath"] = assetPath;
        j["visible"] = visible;
    }

    void deserialize(const json& j) {
        if (j.contains("assetPath")) j.at("assetPath").get_to(assetPath);
        if (j.contains("visible")) j.at("visible").get_to(visible);
    }
};

/** @brief Camera Component (Optical data). */
struct CameraComponent {
    Ref<Camera> camera;
    bool active = true;
    float fov = 45.0f;
    float aspect = 1.77f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;

    CameraComponent() = default;
    CameraComponent(Ref<Camera> c) : camera(c) {
        // Ideally we would retrieve camera values from the camera object here
    }

    void serialize(json& j) const {
        j["active"] = active;
        j["fov"] = fov;
        j["aspect"] = aspect;
        j["nearPlane"] = nearPlane;
        j["farPlane"] = farPlane;
    }

    void deserialize(const json& j) {
        if (j.contains("active")) j.at("active").get_to(active);
        if (j.contains("fov")) j.at("fov").get_to(fov);
        if (j.contains("aspect")) j.at("aspect").get_to(aspect);
        if (j.contains("nearPlane")) j.at("nearPlane").get_to(nearPlane);
        if (j.contains("farPlane")) j.at("farPlane").get_to(farPlane);
    }
};

/** @brief Controller for FPS camera (Keyboard/Mouse). */
struct FPSControllerComponent {
    glm::vec3 movementSpeed = {10.0f, 10.0f, 10.0f};
    glm::vec2 rotationSpeed = {0.1f, 0.1f};
    
    // Internal state (Yaw/Pitch)
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

/** @brief Controller for Orbital camera. */
struct OrbitControllerComponent {
    glm::vec3 target = {0.0f, 0.0f, 0.0f};
    float distance = 10.0f;
    float minDistance = 1.0f;
    float maxDistance = 100.0f;
    
    glm::vec2 rotationSpeed = {0.2f, 0.2f};
    float zoomSpeed = 0.5f;

    // Internal state
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

/** @brief Light Component. */
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

/** @brief RigidBody Physics Component. */
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
    std::string assetPath; // Path of the mesh used as collider

    void serialize(json& j) const { j["convex"] = convex; j["assetPath"] = assetPath; }
    void deserialize(const json& j) { 
        if (j.contains("convex")) j.at("convex").get_to(convex);
        if (j.contains("assetPath")) j.at("assetPath").get_to(assetPath);
    }
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

/** @brief Audio Source Component. */
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

/** @brief Terrain Component. */
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

/** @brief Particle System Component. */
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

/** @brief Skybox Component (Cubemap). */
struct SkyboxComponent {
    Ref<Texture> cubemap;
    std::string assetPath;
    void serialize(json& j) const { j["assetPath"] = assetPath; }
    void deserialize(const json& j) { if (j.contains("assetPath")) j.at("assetPath").get_to(assetPath); }
};

/** @brief SkySphere Component (Equirectangular 2D texture). */
struct SkySphereComponent {
    Ref<Texture> texture;
    std::string assetPath;
    void serialize(json& j) const { j["assetPath"] = assetPath; }
    void deserialize(const json& j) { if (j.contains("assetPath")) j.at("assetPath").get_to(assetPath); }
};

/** @brief Component for simple procedural animations. */
struct SimpleAnimationComponent {
    bool active = true;
    SimpleAnimationType type = SimpleAnimationType::Rotation;
    float speed = 1.0f;
    bool physicsSync = true;

    // Rotation parameters (Local)
    glm::vec3 rotationAxis = { 0.0f, 1.0f, 0.0f };

    // Translation/Oscillation parameters (Local)
    glm::vec3 direction = { 0.0f, 1.0f, 0.0f };
    float amplitude = 1.0f;
    bool pingPong = true;

    // Waypoints parameters (World)
    std::vector<glm::vec3> waypoints;
    int currentWaypoint = 0;
    bool loop = true;

    // Internal state (non-serialized)
    float timeAccumulator = 0.0f;
    bool initialized = false;

    void serialize(json& j) const {
        j["active"] = active;
        j["type"] = static_cast<int>(type);
        j["speed"] = speed;
        j["physicsSync"] = physicsSync;
        j["rotationAxis"] = rotationAxis;
        j["direction"] = direction;
        j["amplitude"] = amplitude;
        j["pingPong"] = pingPong;
        j["waypoints"] = waypoints;
        j["currentWaypoint"] = currentWaypoint;
        j["loop"] = loop;
    }

    void deserialize(const json& j) {
        if (j.contains("active")) j.at("active").get_to(active);
        if (j.contains("type")) type = static_cast<SimpleAnimationType>(j.at("type").get<int>());
        if (j.contains("speed")) j.at("speed").get_to(speed);
        if (j.contains("physicsSync")) j.at("physicsSync").get_to(physicsSync);
        if (j.contains("rotationAxis")) j.at("rotationAxis").get_to(rotationAxis);
        if (j.contains("direction")) j.at("direction").get_to(direction);
        if (j.contains("amplitude")) j.at("amplitude").get_to(amplitude);
        if (j.contains("pingPong")) j.at("pingPong").get_to(pingPong);
        if (j.contains("waypoints")) j.at("waypoints").get_to(waypoints);
        if (j.contains("currentWaypoint")) j.at("currentWaypoint").get_to(currentWaypoint);
        if (j.contains("loop")) j.at("loop").get_to(loop);
    }
};

/** @brief Component to attach a behavior (C++ script) to an entity. */
struct NativeScriptComponent {
    std::function<void(Entity, float)> onUpdate;

    NativeScriptComponent() = default;
    
    template<typename Func>
    NativeScriptComponent(Func&& func) : onUpdate(std::forward<Func>(func)) {}

    // Native scripts (function pointers) are not serializable by default
    void serialize(json&) const {} 
    void deserialize(const json&) {}
};

} // namespace bb3d
