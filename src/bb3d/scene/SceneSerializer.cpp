#include "bb3d/scene/SceneSerializer.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/core/Log.hpp"
#include <fstream>
#include <iomanip>

namespace bb3d {

    SceneSerializer::SceneSerializer(Ref<Scene> scene) : m_scene(scene) {}

    static void SerializeEntity(json& j, Entity entity) {
        if (entity.has<TagComponent>()) {
            json c;
            entity.get<TagComponent>().serialize(c);
            j["TagComponent"] = c;
        }

        if (entity.has<TransformComponent>()) {
            json c;
            entity.get<TransformComponent>().serialize(c);
            j["TransformComponent"] = c;
        }

        if (entity.has<MeshComponent>()) {
            json c;
            entity.get<MeshComponent>().serialize(c);
            j["MeshComponent"] = c;
        }

        if (entity.has<ModelComponent>()) {
            json c;
            entity.get<ModelComponent>().serialize(c);
            j["ModelComponent"] = c;
        }

        if (entity.has<CameraComponent>()) {
            json c;
            entity.get<CameraComponent>().serialize(c);
            j["CameraComponent"] = c;
        }

        if (entity.has<LightComponent>()) {
            json c;
            entity.get<LightComponent>().serialize(c);
            j["LightComponent"] = c;
        }

        if (entity.has<RigidBodyComponent>()) {
            json c;
            entity.get<RigidBodyComponent>().serialize(c);
            j["RigidBodyComponent"] = c;
        }

        // ... Continuer pour les autres composants (Audio, Terrain, etc.)
    }

    void SceneSerializer::serialize(std::string_view filepath) {
        json root;
        root["Scene"] = "Untitled Scene";
        
        // Environment
        json env;
        if (m_scene->getSkybox()) env["Skybox"] = "TODO: Path"; // Il faudrait stocker le chemin dans Texture
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
            std::string name;
            if (e.contains("TagComponent")) name = e["TagComponent"]["tag"];

            Entity entity = m_scene->createEntity(name);

            if (e.contains("TransformComponent")) {
                auto& tc = entity.get<TransformComponent>();
                tc.deserialize(e["TransformComponent"]);
            }

            if (e.contains("MeshComponent")) {
                entity.add<MeshComponent>();
                entity.get<MeshComponent>().deserialize(e["MeshComponent"]);
            }

            if (e.contains("LightComponent")) {
                entity.add<LightComponent>();
                entity.get<LightComponent>().deserialize(e["LightComponent"]);
            }

            // ... etc
        }

        BB_CORE_INFO("SceneSerializer: Deserialized scene from {0}", filepath);
        return true;
    }

} // namespace bb3d
