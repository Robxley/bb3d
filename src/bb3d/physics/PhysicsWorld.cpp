#include "bb3d/physics/PhysicsWorld.hpp"
#include "bb3d/core/Log.hpp"
#include "bb3d/scene/Scene.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/scene/Components.hpp"

// Jolt includes
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>

#include <thread>
#include <unordered_map>

namespace bb3d {

    namespace Layers {
        static constexpr JPH::ObjectLayer NON_MOVING = 0;
        static constexpr JPH::ObjectLayer MOVING = 1;
        static constexpr uint32_t NUM_LAYERS = 2;
    }

    namespace BroadPhaseLayers {
        static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
        static constexpr JPH::BroadPhaseLayer MOVING(1);
        static constexpr uint32_t NUM_LAYERS = 2;
    }

    // --- Helpers ---
    inline JPH::Vec3 toJPH(const glm::vec3& v) { return JPH::Vec3(v.x, v.y, v.z); }
    inline JPH::Float3 toJPHFloat3(const glm::vec3& v) { return JPH::Float3(v.x, v.y, v.z); }
    inline glm::vec3 fromJPH(const JPH::RVec3& v) { return glm::vec3(v.GetX(), v.GetY(), v.GetZ()); }
    inline JPH::Quat toJPH(const glm::quat& q) { return JPH::Quat(q.x, q.y, q.z, q.w); }
    inline glm::quat fromJPH(const JPH::Quat& q) { return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ()); }

    // --- Filter Implementations ---

    class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
    public:
        bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override {
            switch (inObject1) {
                case Layers::NON_MOVING: return inObject2 == Layers::MOVING;
                case Layers::MOVING: return true;
                default: return false;
            }
        }
    };

    class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
    public:
        BPLayerInterfaceImpl() {
            mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
            mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
        }
        uint32_t GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }
        JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override { return mObjectToBroadPhase[inLayer]; }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
            if (inLayer == BroadPhaseLayers::NON_MOVING) return "NON_MOVING";
            if (inLayer == BroadPhaseLayers::MOVING) return "MOVING";
            return "INVALID";
        }
#endif
    private:
        JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
    };

    class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
    public:
        bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
            switch (inLayer1) {
                case Layers::NON_MOVING: return inLayer2 == BroadPhaseLayers::MOVING;
                case Layers::MOVING: return true;
                default: return false;
            }
        }
    };

    // --- PIMPL Impl ---

    struct PhysicsWorld::Impl {
        bool initialized = false;
        Scope<JPH::PhysicsSystem> physicsSystem;
        Scope<JPH::TempAllocatorImpl> tempAllocator;
        Scope<JPH::JobSystemThreadPool> jobSystem;

        BPLayerInterfaceImpl bpLayerInterface;
        ObjectVsBroadPhaseLayerFilterImpl objVsBpFilter;
        ObjectLayerPairFilterImpl objLayerPairFilter;

        std::unordered_map<uint32_t, JPH::Ref<JPH::CharacterVirtual>> characters;
    };

    // --- PhysicsWorld Methods ---

    PhysicsWorld::PhysicsWorld() : m_impl(CreateScope<Impl>()) {}
    PhysicsWorld::~PhysicsWorld() { shutdown(); }

    void PhysicsWorld::init() {
        BB_CORE_INFO("PhysicsWorld: Initializing Jolt Physics...");

        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();

        m_impl->tempAllocator = CreateScope<JPH::TempAllocatorImpl>(10 * 1024 * 1024);
        m_impl->jobSystem = CreateScope<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, (int)std::thread::hardware_concurrency() - 1);

        m_impl->physicsSystem = CreateScope<JPH::PhysicsSystem>();
        m_impl->physicsSystem->Init(1024, 0, 1024, 1024, m_impl->bpLayerInterface, m_impl->objVsBpFilter, m_impl->objLayerPairFilter);

        m_impl->initialized = true;
        BB_CORE_INFO("PhysicsWorld: Jolt Physics initialized.");
    }

    void PhysicsWorld::update(float deltaTime, Scene& scene) {
        if (!m_impl->initialized) return;

        // 1. Synchro "Kinematic" : Moteur -> Jolt
        // Pour les objets cinématiques (déplacés par le code/animation), on force leur position dans Jolt.
        auto& bodyInterface = m_impl->physicsSystem->GetBodyInterface();
        auto kinematicView = scene.getRegistry().view<RigidBodyComponent, TransformComponent>();
        for (auto entity : kinematicView) {
            auto& rb = kinematicView.get<RigidBodyComponent>(entity);
            auto& tf = kinematicView.get<TransformComponent>(entity);
            if (rb.bodyID == 0xFFFFFFFF || rb.type != BodyType::Kinematic) continue;
            bodyInterface.SetPositionAndRotation(JPH::BodyID(rb.bodyID), toJPH(tf.translation), toJPH(glm::quat(tf.rotation)), JPH::EActivation::Activate);
        }

        // 2. Mise à jour des personnages (CharacterVirtual)
        // Les personnages utilisent une logique spécifique car ils ne sont pas des RigidBodies standards.
        for (auto it = m_impl->characters.begin(); it != m_impl->characters.end(); ) {
            entt::entity handle = static_cast<entt::entity>(it->first);
            if (!scene.getRegistry().valid(handle)) { it = m_impl->characters.erase(it); continue; }

            auto& charV = it->second;
            auto& cc = scene.getRegistry().get<CharacterControllerComponent>(handle);
            auto& tf = scene.getRegistry().get<TransformComponent>(handle);

            charV->SetLinearVelocity(toJPH(cc.velocity));
            JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;
            charV->ExtendedUpdate(deltaTime, m_impl->physicsSystem->GetGravity(), updateSettings, 
                                 m_impl->physicsSystem->GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
                                 m_impl->physicsSystem->GetDefaultLayerFilter(Layers::MOVING), {}, {}, *m_impl->tempAllocator);

            // Retour de la position physique vers le moteur
            tf.translation = fromJPH(charV->GetPosition());
            tf.rotation = glm::eulerAngles(fromJPH(charV->GetRotation()));
            cc.isGrounded = charV->IsSupported();
            cc.velocity = fromJPH(charV->GetLinearVelocity());
            ++it;
        }

        // 3. Step de la simulation (Calcul des collisions et forces)
        m_impl->physicsSystem->Update(deltaTime, 1, m_impl->tempAllocator.get(), m_impl->jobSystem.get());

        // Scan for new RigidBodies without Jolt ID and create them
        auto newBodiesView = scene.getRegistry().view<RigidBodyComponent>();
        for (auto entityHandle : newBodiesView) {
            auto& rb = newBodiesView.get<RigidBodyComponent>(entityHandle);
            if (rb.bodyID == 0xFFFFFFFF) {
                createRigidBody(Entity(entityHandle, scene));
            }
        }

        // 4. Synchro "Dynamic" : Jolt -> Moteur
        // Pour les objets dynamiques (tombant par gravité, etc.), on récupère la position calculée.
        syncTransforms(scene);
    }

    void PhysicsWorld::syncTransforms(Scene& scene) {
        if (!m_impl->initialized) return;
        auto& bodyInterface = m_impl->physicsSystem->GetBodyInterface();
        auto view = scene.getRegistry().view<RigidBodyComponent, TransformComponent>();

        for (auto entity : view) {
            auto& rb = view.get<RigidBodyComponent>(entity);
            auto& tf = view.get<TransformComponent>(entity);

            if (rb.bodyID == 0xFFFFFFFF || rb.type == BodyType::Kinematic || rb.type == BodyType::Static) continue;

            JPH::BodyID id(rb.bodyID);
            if (bodyInterface.IsActive(id)) {
                JPH::RVec3 pos; JPH::Quat rot;
                bodyInterface.GetPositionAndRotation(id, pos, rot);
                tf.translation = fromJPH(pos);
                tf.rotation = glm::eulerAngles(fromJPH(rot));
            }
        }
    }

    void PhysicsWorld::createRigidBody(Entity entity) {
        if (!m_impl->initialized || !entity.has<RigidBodyComponent>()) return;

        auto& rb = entity.get<RigidBodyComponent>();
        auto& tf = entity.get<TransformComponent>();
        auto& bodyInterface = m_impl->physicsSystem->GetBodyInterface();

        JPH::ShapeRefC shape;
        if (entity.has<BoxColliderComponent>()) {
            shape = new JPH::BoxShape(toJPH(entity.get<BoxColliderComponent>().halfExtents * tf.scale));
        } else if (entity.has<SphereColliderComponent>()) {
            shape = new JPH::SphereShape(entity.get<SphereColliderComponent>().radius * glm::max(tf.scale.x, glm::max(tf.scale.y, tf.scale.z)));
        } else if (entity.has<CapsuleColliderComponent>()) {
            auto& cap = entity.get<CapsuleColliderComponent>();
            shape = new JPH::CapsuleShape(cap.height * 0.5f * tf.scale.y, cap.radius * glm::max(tf.scale.x, tf.scale.z));
        } else if (entity.has<MeshColliderComponent>()) {
            auto& mc = entity.get<MeshColliderComponent>();
            if (mc.mesh) {
#if defined(BB3D_DEBUG)
                if (mc.mesh->isCPUDataReleased()) {
                    BB_CORE_ERROR("PhysicsWorld: Cannot create MeshCollider for entity '{}', Mesh CPU data has been released! Create colliders BEFORE calling releaseCPUData().", 
                        entity.has<TagComponent>() ? entity.get<TagComponent>().tag : "Unnamed");
                }
#endif
                if (mc.convex) {
                    JPH::ConvexHullShapeSettings settings;
                    for (const auto& v : mc.mesh->getVertices()) settings.mPoints.push_back(toJPH(v.position * tf.scale));
                    shape = settings.Create().Get();
                } else {
                    JPH::VertexList vertices;
                    for (const auto& v : mc.mesh->getVertices()) vertices.push_back(toJPHFloat3(v.position * tf.scale));
                    JPH::IndexedTriangleList triangles;
                    const auto& indices = mc.mesh->getIndices();
                    for (size_t i = 0; i < indices.size(); i += 3) triangles.push_back({indices[i], indices[i+1], indices[i+2]});
                    shape = JPH::MeshShapeSettings(vertices, triangles).Create().Get();
                }
            }
        }

        if (!shape) shape = new JPH::BoxShape(toJPH(tf.scale * 0.5f));

        JPH::EMotionType motion = JPH::EMotionType::Static;
        JPH::ObjectLayer layer = Layers::NON_MOVING;
        if (rb.type == BodyType::Dynamic) { motion = JPH::EMotionType::Dynamic; layer = Layers::MOVING; }
        else if (rb.type == BodyType::Kinematic) { motion = JPH::EMotionType::Kinematic; layer = Layers::MOVING; }

        JPH::BodyCreationSettings settings(shape, toJPH(tf.translation), toJPH(glm::quat(tf.rotation)), motion, layer);
        settings.mFriction = rb.friction;
        settings.mRestitution = rb.restitution;
        settings.mLinearVelocity = toJPH(rb.initialLinearVelocity);
        settings.mMassPropertiesOverride.mMass = rb.mass;
        settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;

        JPH::Body* body = bodyInterface.CreateBody(settings);
        bodyInterface.AddBody(body->GetID(), JPH::EActivation::Activate);
        rb.bodyID = body->GetID().GetIndexAndSequenceNumber();
    }

    void PhysicsWorld::createCharacterController(Entity entity) {
        if (!m_impl->initialized || !entity.has<CharacterControllerComponent>()) return;

        auto& tf = entity.get<TransformComponent>();
        auto& cc = entity.get<CharacterControllerComponent>();

        JPH::ShapeRefC shape;
        if (entity.has<CapsuleColliderComponent>()) {
            auto& cap = entity.get<CapsuleColliderComponent>();
            shape = new JPH::CapsuleShape(cap.height * 0.5f * tf.scale.y, cap.radius * glm::max(tf.scale.x, tf.scale.z));
        } else {
            shape = new JPH::CapsuleShape(0.5f, 0.3f); 
        }

        JPH::CharacterVirtualSettings settings;
        settings.mShape = shape;
        settings.mMaxSlopeAngle = glm::radians(cc.maxSlopeAngle);
        
        auto character = new JPH::CharacterVirtual(&settings, toJPH(tf.translation), toJPH(glm::quat(tf.rotation)), m_impl->physicsSystem.get());
        m_impl->characters[static_cast<uint32_t>(entity.getHandle())] = character;
    }

    RaycastResult PhysicsWorld::raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance) {
        RaycastResult res;
        if (!m_impl->initialized) return res;

        JPH::RRayCast ray{ toJPH(origin), toJPH(direction * maxDistance) };
        JPH::RayCastResult joltRes;
        if (m_impl->physicsSystem->GetNarrowPhaseQuery().CastRay(ray, joltRes)) {
            res.hit = true; res.fraction = joltRes.mFraction;
            res.bodyID = joltRes.mBodyID.GetIndexAndSequenceNumber();
            res.position = origin + direction * maxDistance * joltRes.mFraction;
            JPH::BodyLockRead lock(m_impl->physicsSystem->GetBodyLockInterface(), joltRes.mBodyID);
            if (lock.Succeeded()) res.normal = fromJPH(lock.GetBody().GetWorldSpaceSurfaceNormal(joltRes.mSubShapeID2, ray.GetPointOnRay(joltRes.mFraction)));
        }
        return res;
    }

    void PhysicsWorld::shutdown() {
        if (!m_impl->initialized) return;
        BB_CORE_INFO("PhysicsWorld: Shutting down Jolt Physics...");
        m_impl->characters.clear();
        m_impl->physicsSystem.reset();
        m_impl->jobSystem.reset();
        m_impl->tempAllocator.reset();
        JPH::UnregisterTypes();
        if (JPH::Factory::sInstance) { delete JPH::Factory::sInstance; JPH::Factory::sInstance = nullptr; }
        m_impl->initialized = false;
    }

} // namespace bb3d