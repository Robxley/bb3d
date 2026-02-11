#include "bb3d/scene/SceneSerializer.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/core/Engine.hpp"
#include "bb3d/physics/PhysicsWorld.hpp"
#include "bb3d/core/Log.hpp"
#include <fstream>
#include <iomanip>

namespace bb3d {

    SceneSerializer::SceneSerializer(Ref<Scene> scene) : m_scene(scene) {}

    template<typename T>
    static void SerializeComponent(json& j, Entity entity, const std::string& name) {
        if (entity.has<T>()) {
            json c;
            entity.get<T>().serialize(c);
            j[name] = c;
        }
    }

    static void SerializeEntity(json& j, Entity entity) {
        SerializeComponent<TagComponent>(j, entity, "TagComponent");
        SerializeComponent<TransformComponent>(j, entity, "TransformComponent");
        SerializeComponent<MeshComponent>(j, entity, "MeshComponent");
        SerializeComponent<ModelComponent>(j, entity, "ModelComponent");
        SerializeComponent<CameraComponent>(j, entity, "CameraComponent");
        SerializeComponent<FPSControllerComponent>(j, entity, "FPSControllerComponent");
        SerializeComponent<OrbitControllerComponent>(j, entity, "OrbitControllerComponent");
        SerializeComponent<LightComponent>(j, entity, "LightComponent");
        SerializeComponent<RigidBodyComponent>(j, entity, "RigidBodyComponent");
        SerializeComponent<BoxColliderComponent>(j, entity, "BoxColliderComponent");
        SerializeComponent<SphereColliderComponent>(j, entity, "SphereColliderComponent");
        SerializeComponent<CapsuleColliderComponent>(j, entity, "CapsuleColliderComponent");
        SerializeComponent<MeshColliderComponent>(j, entity, "MeshColliderComponent");
        SerializeComponent<SkySphereComponent>(j, entity, "SkySphereComponent");
        SerializeComponent<SkyboxComponent>(j, entity, "SkyboxComponent");
        SerializeComponent<AudioSourceComponent>(j, entity, "AudioSourceComponent");
        SerializeComponent<AudioListenerComponent>(j, entity, "AudioListenerComponent");
    }

    void SceneSerializer::serialize(std::string_view filepath) {
        json root;
        root["Scene"] = "Untitled Scene";
        
        // Environment
        json env;
        env["Fog"] = {
            {"color", m_scene->getFog().color},
            {"density", m_scene->getFog().density},
            {"type", static_cast<int>(m_scene->getFog().type)}
        };
        root["Environment"] = env;

        json entities = json::array();
        for (auto entityID : m_scene->m_registry.storage<entt::entity>()) {
            Entity entity(entityID, *m_scene);
            if (!entity) continue;

            json e;
            SerializeEntity(e, entity);
            entities.push_back(e);
        }

        root["Entities"] = entities;

        std::ofstream fout(filepath.data());
        if (!fout.is_open()) {
            BB_CORE_ERROR("SceneSerializer: Failed to open file for writing: {0}", filepath);
            return;
        }
        fout << std::setw(4) << root << std::endl;
        BB_CORE_INFO("SceneSerializer: Serialized scene to {0}", filepath);
    }

    bool SceneSerializer::deserialize(std::string_view filepath) {
        std::ifstream stream(filepath.data());
        if (!stream.is_open()) return false;

        json data;
        try {
            stream >> data;
        } catch (const std::exception& e) {
            BB_CORE_ERROR("SceneSerializer: Failed to parse JSON: {0}", e.what());
            return false;
        }

        Engine* engine = m_scene->getEngineContext();
        if (!engine) {
            BB_CORE_ERROR("SceneSerializer: Scene has no engine context, cannot reload assets!");
            return false;
        }

        m_scene->clear();

        if (data.contains("Environment")) {
            auto env = data["Environment"];
            if (env.contains("Fog")) {
                FogSettings fog;
                if (env["Fog"].contains("color")) env["Fog"]["color"].get_to(fog.color);
                if (env["Fog"].contains("density")) env["Fog"]["density"].get_to(fog.density);
                if (env["Fog"].contains("type")) fog.type = static_cast<FogType>(env["Fog"]["type"].get<int>());
                m_scene->setFog(fog);
            }
        }

        if (!data.contains("Entities")) return false;

        auto entities = data["Entities"];
        for (auto& e : entities) {
            std::string name = "Entity";
            if (e.contains("TagComponent")) name = e["TagComponent"]["tag"];

            Entity entity = m_scene->createEntity(name);

            if (e.contains("TransformComponent")) {
                entity.get<TransformComponent>().deserialize(e["TransformComponent"]);
            }

            if (e.contains("MeshComponent")) {
                auto& mc = entity.add<MeshComponent>().get<MeshComponent>();
                mc.deserialize(e["MeshComponent"]);
                if (!mc.assetPath.empty()) {
                    try { mc.mesh = engine->assets().load<Mesh>(mc.assetPath); } catch(...) {}
                }
            }

            if (e.contains("ModelComponent")) {
                auto& mc = entity.add<ModelComponent>().get<ModelComponent>();
                mc.deserialize(e["ModelComponent"]);
                if (!mc.assetPath.empty()) {
                    try { mc.model = engine->assets().load<Model>(mc.assetPath); } catch(...) {}
                }
            }

            if (e.contains("LightComponent")) {
                entity.add<LightComponent>().get<LightComponent>().deserialize(e["LightComponent"]);
            }

            if (e.contains("RigidBodyComponent")) {
                entity.add<RigidBodyComponent>().get<RigidBodyComponent>().deserialize(e["RigidBodyComponent"]);
            }

            if (e.contains("BoxColliderComponent")) entity.add<BoxColliderComponent>().get<BoxColliderComponent>().deserialize(e["BoxColliderComponent"]);
            if (e.contains("SphereColliderComponent")) entity.add<SphereColliderComponent>().get<SphereColliderComponent>().deserialize(e["SphereColliderComponent"]);
            if (e.contains("CapsuleColliderComponent")) entity.add<CapsuleColliderComponent>().get<CapsuleColliderComponent>().deserialize(e["CapsuleColliderComponent"]);
            if (e.contains("MeshColliderComponent")) {
                auto& mc = entity.add<MeshColliderComponent>().get<MeshColliderComponent>();
                mc.deserialize(e["MeshColliderComponent"]);
                if (!mc.assetPath.empty()) {
                    try { mc.mesh = engine->assets().load<Mesh>(mc.assetPath); } catch(...) {}
                }
            }

            if (e.contains("FPSControllerComponent")) entity.add<FPSControllerComponent>().get<FPSControllerComponent>().deserialize(e["FPSControllerComponent"]);
            if (e.contains("OrbitControllerComponent")) entity.add<OrbitControllerComponent>().get<OrbitControllerComponent>().deserialize(e["OrbitControllerComponent"]);
            
            if (e.contains("CameraComponent")) {
                auto& cc = entity.add<CameraComponent>().get<CameraComponent>();
                cc.deserialize(e["CameraComponent"]);
                cc.camera = CreateRef<Camera>(cc.fov, cc.aspect, cc.nearPlane, cc.farPlane);
            }

            if (e.contains("SkySphereComponent")) {
                auto& ss = entity.add<SkySphereComponent>().get<SkySphereComponent>();
                ss.deserialize(e["SkySphereComponent"]);
                if (!ss.assetPath.empty()) {
                    try { ss.texture = engine->assets().load<Texture>(ss.assetPath, true); } catch(...) {}
                }
            }

            // ... etc
        }

        // --- CRITICAL: PHYSICS RECONSTRUCTION ---
        if (engine->GetPhysicsWorld()) {
            auto view = m_scene->getRegistry().view<RigidBodyComponent>();
            for (auto entityID : view) {
                engine->physics().createRigidBody(Entity(entityID, *m_scene));
            }
        }

        BB_CORE_INFO("SceneSerializer: Deserialized scene from {0}", filepath);
        return true;
    }

} // namespace bb3d
