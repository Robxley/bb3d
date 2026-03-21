#include "bb3d/scene/SceneSerializer.hpp"
#include "bb3d/scene/ComponentRegistry.hpp"
#include "bb3d/render/MeshGenerator.hpp"
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
        SerializeComponent<PhysicsComponent>(j, entity, "PhysicsComponent");
        SerializeComponent<SkySphereComponent>(j, entity, "SkySphereComponent");
        SerializeComponent<SkyboxComponent>(j, entity, "SkyboxComponent");
        SerializeComponent<AudioSourceComponent>(j, entity, "AudioSourceComponent");
        SerializeComponent<AudioListenerComponent>(j, entity, "AudioListenerComponent");
        SerializeComponent<ParticleSystemComponent>(j, entity, "ParticleSystemComponent");
        SerializeComponent<ProceduralPlanetComponent>(j, entity, "ProceduralPlanetComponent");
        SerializeComponent<PointGravitySourceComponent>(j, entity, "PointGravitySourceComponent");

        auto& customHandlers = ComponentRegistry::GetHandlers();
        for (const auto& [compName, handler] : customHandlers) {
            handler.serialize(entity, j);
        }
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
        std::ifstream stream(std::string(filepath).c_str());
        if (!stream.is_open()) {
            BB_CORE_ERROR("SceneSerializer: Failed to open file for reading: {0}", filepath);
            return false;
        }

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

        if (!data.contains("Entities")) {
            BB_CORE_ERROR("SceneSerializer: JSON does not contain 'Entities' key!");
            return false;
        }

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
                } else if (mc.primitiveType != PrimitiveType::None) {
                    // Reconstruct primitives
                    if (mc.primitiveType == PrimitiveType::Cube) mc.mesh = MeshGenerator::createCube(engine->graphics(), 1.0f, mc.color);
                    else if (mc.primitiveType == PrimitiveType::Sphere) mc.mesh = MeshGenerator::createSphere(engine->graphics(), 0.5f, 32, mc.color);
                    else if (mc.primitiveType == PrimitiveType::Plane) mc.mesh = MeshGenerator::createCheckerboardPlane(engine->graphics(), 10.0f);
                    
                    // If we recreated a mesh, we also give it a PBR material with the right color
                    auto pbr = CreateRef<PBRMaterial>(engine->graphics());
                    pbr->setColor(mc.color);
                    mc.mesh->setMaterial(pbr);
                }
            }

            if (e.contains("ModelComponent")) {
                auto& mc = entity.add<ModelComponent>().get<ModelComponent>();
                mc.deserialize(e["ModelComponent"]);
                if (!mc.assetPath.empty()) {
                    try { mc.model = engine->assets().load<Model>(mc.assetPath); } catch(...) {}
                }
            }

            if (e.contains("NativeScriptComponent")) entity.add<NativeScriptComponent>().get<NativeScriptComponent>().deserialize(e["NativeScriptComponent"]);
            if (e.contains("PhysicsComponent")) {
                entity.add<PhysicsComponent>().get<PhysicsComponent>().deserialize(e["PhysicsComponent"]);
                // Note: The physical body in Jolt must be recreated after loading all components
            }

            if (e.contains("LightComponent")) {
                entity.add<LightComponent>().get<LightComponent>().deserialize(e["LightComponent"]);
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

            if (e.contains("SkyboxComponent")) {
                auto& sb = entity.add<SkyboxComponent>().get<SkyboxComponent>();
                sb.deserialize(e["SkyboxComponent"]);
                if (!sb.assetPath.empty()) {
                    try { sb.cubemap = engine->assets().load<Texture>(sb.assetPath, true); } catch(...) {}
                }
            }

            if (e.contains("AudioSourceComponent")) entity.add<AudioSourceComponent>().get<AudioSourceComponent>().deserialize(e["AudioSourceComponent"]);
            if (e.contains("AudioListenerComponent")) entity.add<AudioListenerComponent>().get<AudioListenerComponent>().deserialize(e["AudioListenerComponent"]);
            if (e.contains("ParticleSystemComponent")) entity.add<ParticleSystemComponent>().get<ParticleSystemComponent>().deserialize(e["ParticleSystemComponent"]);
            
            if (e.contains("ProceduralPlanetComponent")) {
                entity.add<ProceduralPlanetComponent>().get<ProceduralPlanetComponent>().deserialize(e["ProceduralPlanetComponent"]);
            }
            if (e.contains("PointGravitySourceComponent")) {
                entity.add<PointGravitySourceComponent>().get<PointGravitySourceComponent>().deserialize(e["PointGravitySourceComponent"]);
            }

            auto& customHandlers = ComponentRegistry::GetHandlers();
            for (const auto& [compName, handler] : customHandlers) {
                handler.deserialize(entity, e);
            }
        }

        // Re-create Physics Components
        if (engine->GetPhysicsWorld()) {
            auto view = m_scene->getRegistry().view<PhysicsComponent>();
            for (auto entityID : view) {
                engine->physics().createRigidBody(Entity(entityID, *m_scene));
            }
        }

        BB_CORE_INFO("SceneSerializer: Deserialized scene from {0}", filepath);
        return true;
    }

} // namespace bb3d
