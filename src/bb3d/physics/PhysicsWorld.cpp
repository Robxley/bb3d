#include "bb3d/physics/PhysicsWorld.hpp"
#include "bb3d/core/Log.hpp"
#include "bb3d/scene/Scene.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/scene/Components.hpp"
#include <algorithm>

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

        if (!JPH::Factory::sInstance) {
            JPH::RegisterDefaultAllocator();
            JPH::Factory::sInstance = new JPH::Factory();
            JPH::RegisterTypes();
        }

        m_impl->tempAllocator = CreateScope<JPH::TempAllocatorImpl>(10 * 1024 * 1024);
        m_impl->jobSystem = CreateScope<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, (int)std::thread::hardware_concurrency() - 1);

        m_impl->physicsSystem = CreateScope<JPH::PhysicsSystem>();
        m_impl->physicsSystem->Init(1024, 0, 1024, 1024, m_impl->bpLayerInterface, m_impl->objVsBpFilter, m_impl->objLayerPairFilter);

        m_impl->initialized = true;
        BB_CORE_INFO("PhysicsWorld: Jolt Physics initialized.");
    }

    void PhysicsWorld::setGravity(const glm::vec3& gravity) {
        if (!m_impl->initialized) return;
        m_impl->physicsSystem->SetGravity(toJPH(gravity));
    }

    void PhysicsWorld::update(float deltaTime, Scene& scene) {
        if (!m_impl->initialized) return;

        // 1. "Kinematic" sync: Engine -> Jolt
        // For kinematic objects (moved by code/animation), we force their position in Jolt.
        auto& bodyInterface = m_impl->physicsSystem->GetBodyInterface();
        auto kinematicView = scene.getRegistry().view<PhysicsComponent, TransformComponent>();
        for (auto entity : kinematicView) {
            auto& phys = kinematicView.get<PhysicsComponent>(entity);
            if (phys.type == BodyType::Kinematic && phys.bodyID != 0xFFFFFFFF) {
                auto& tf = kinematicView.get<TransformComponent>(entity);
                bodyInterface.SetPositionAndRotation(JPH::BodyID(phys.bodyID), toJPH(tf.translation), toJPH(glm::quat(tf.rotation)), JPH::EActivation::Activate);
            }
        }

        // 2. Character updates (CharacterVirtual)
        // Characters use specific logic as they are not standard RigidBodies.
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

            // Output physical position back to engine
            tf.translation = fromJPH(charV->GetPosition());
            tf.rotation = glm::eulerAngles(fromJPH(charV->GetRotation()));
            cc.isGrounded = charV->IsSupported();
            cc.velocity = fromJPH(charV->GetLinearVelocity());
            ++it;
        }

        // 3. Simulation Step — Fixed-timestep accumulator for stability
        //    Jolt requires consistent time steps to prevent tunneling and jitter.
        //    We accumulate real time and step in fixed increments.
        constexpr float fixedDt = 1.0f / 60.0f;        // 60 Hz physics
        constexpr int maxSubSteps = 4;                  // Cap to prevent spiral of death
        static float accumulator = 0.0f;
        accumulator += std::min(deltaTime, 0.1f);       // Clamp input to 100ms max
        int steps = 0;
        while (accumulator >= fixedDt && steps < maxSubSteps) {
            m_impl->physicsSystem->Update(fixedDt, 1, m_impl->tempAllocator.get(), m_impl->jobSystem.get());
            accumulator -= fixedDt;
            steps++;
        }

        // 3. Create missing Jolt bodies (for new components added this frame)
        auto newBodiesView = scene.getRegistry().view<PhysicsComponent>();
        for (auto entityHandle : newBodiesView) {
            auto& phys = newBodiesView.get<PhysicsComponent>(entityHandle);
            if (phys.bodyID == 0xFFFFFFFF) {
                createRigidBody(Entity(entityHandle, scene));
            }
        }

        // 4. "Dynamic" sync: Jolt -> Engine
        // For dynamic objects (falling by gravity, etc.), we retrieve the calculated position.
        syncTransforms(scene);
    }

    void PhysicsWorld::syncTransforms(Scene& scene) {
        if (!m_impl->initialized) return;
        auto& bodyInterface = m_impl->physicsSystem->GetBodyInterface();
        auto view = scene.getRegistry().view<PhysicsComponent, TransformComponent>();

        for (auto entity : view) {
            auto& phys = view.get<PhysicsComponent>(entity);
            auto& tf = view.get<TransformComponent>(entity);

            if (phys.bodyID == 0xFFFFFFFF || phys.type == BodyType::Kinematic || phys.type == BodyType::Static) continue;

            JPH::BodyID id(phys.bodyID);
            if (bodyInterface.IsActive(id)) {
                JPH::RVec3 pos; JPH::Quat rot;
                bodyInterface.GetPositionAndRotation(id, pos, rot);
                tf.translation = fromJPH(pos);
                tf.rotation = glm::eulerAngles(fromJPH(rot));
            }
        }
    }

    void PhysicsWorld::updateBodyTransform(Entity entity) {
        if (!m_impl->initialized || !entity.has<PhysicsComponent>() || !entity.has<TransformComponent>()) return;

        auto& phys = entity.get<PhysicsComponent>();
        auto& tf = entity.get<TransformComponent>();
        if (phys.bodyID == 0xFFFFFFFF) return;

        auto& bodyInterface = m_impl->physicsSystem->GetBodyInterface();
        JPH::BodyID id(phys.bodyID);
        
        bodyInterface.SetPositionAndRotation(id, toJPH(tf.translation), toJPH(glm::quat(tf.rotation)), JPH::EActivation::Activate);
    }

    void PhysicsWorld::resetBody(Entity entity) {
        if (!m_impl->initialized || !entity.has<PhysicsComponent>()) return;

        auto& phys = entity.get<PhysicsComponent>();
        if (phys.bodyID == 0xFFFFFFFF) return;

        auto& bodyInterface = m_impl->physicsSystem->GetBodyInterface();
        JPH::BodyID id(phys.bodyID);

        bodyInterface.SetLinearVelocity(id, toJPH(phys.initialLinearVelocity));
        bodyInterface.SetAngularVelocity(id, JPH::Vec3::sZero());
        
        if (entity.has<TransformComponent>()) {
            auto& tf = entity.get<TransformComponent>();
            bodyInterface.SetPositionAndRotation(id, toJPH(tf.translation), toJPH(glm::quat(tf.rotation)), JPH::EActivation::Activate);
        }
    }

    void PhysicsWorld::resetAllBodies(Scene& scene) {
        if (!m_impl->initialized) return;

        auto view = scene.getRegistry().view<PhysicsComponent>();
        for (auto entityHandle : view) {
            resetBody(Entity(entityHandle, scene));
        }

        // Reset CharacterControllers
        for (auto& [handle, charV] : m_impl->characters) {
            entt::entity entHandle = static_cast<entt::entity>(handle);
            if (scene.getRegistry().all_of<TransformComponent>(entHandle)) {
                auto& tf = scene.getRegistry().get<TransformComponent>(entHandle);
                charV->SetPosition(toJPH(tf.translation));
                charV->SetRotation(toJPH(glm::quat(tf.rotation)));
                charV->SetLinearVelocity(JPH::Vec3::sZero());
            }
        }
    }

    void PhysicsWorld::addForce(Entity entity, const glm::vec3& force) {
        if (!m_impl->initialized || !entity.has<PhysicsComponent>()) return;
        auto& phys = entity.get<PhysicsComponent>();
        if (phys.bodyID == 0xFFFFFFFF) return;
        m_impl->physicsSystem->GetBodyInterface().AddForce(JPH::BodyID(phys.bodyID), toJPH(force));
    }

    void PhysicsWorld::addTorque(Entity entity, const glm::vec3& torque) {
        if (!m_impl->initialized || !entity.has<PhysicsComponent>()) return;
        auto& phys = entity.get<PhysicsComponent>();
        if (phys.bodyID == 0xFFFFFFFF) return;
        m_impl->physicsSystem->GetBodyInterface().AddTorque(JPH::BodyID(phys.bodyID), toJPH(torque));
    }

    void PhysicsWorld::createRigidBody(Entity entity) {
        if (!m_impl->initialized || !entity.has<PhysicsComponent>()) return;

        auto& phys = entity.get<PhysicsComponent>();
        auto& tf = entity.get<TransformComponent>();
        auto& bodyInterface = m_impl->physicsSystem->GetBodyInterface();

        JPH::ShapeRefC shape;
        
        if (phys.colliderType == ColliderType::Box) {
            shape = new JPH::BoxShape(toJPH(phys.boxHalfExtents * tf.scale));
        } else if (phys.colliderType == ColliderType::Sphere) {
            shape = new JPH::SphereShape(phys.radius * glm::max(tf.scale.x, glm::max(tf.scale.y, tf.scale.z)));
        } else if (phys.colliderType == ColliderType::Capsule) {
            shape = new JPH::CapsuleShape(phys.height * 0.5f * tf.scale.y, phys.radius * glm::max(tf.scale.x, tf.scale.z));
        } else if (phys.colliderType == ColliderType::Mesh) {
            std::vector<Ref<Mesh>> targetMeshes;
            if (phys.useModelMesh && entity.has<ModelComponent>()) {
                auto& mc = entity.get<ModelComponent>();
                if (mc.model) targetMeshes = mc.model->getMeshes();
            } else if (entity.has<MeshComponent>()) {
                auto& mc = entity.get<MeshComponent>();
                if (mc.mesh) targetMeshes.push_back(mc.mesh);
            } else if (entity.has<ProceduralPlanetComponent>()) {
                auto& planet = entity.get<ProceduralPlanetComponent>();
                if (planet.model) targetMeshes = planet.model->getMeshes();
            } else if (phys.mesh) {
                targetMeshes.push_back(phys.mesh);
            }
            
            if (!targetMeshes.empty()) {
                if (phys.isConvex) {
                    JPH::ConvexHullShapeSettings settings;
                    for (const auto& mesh : targetMeshes) {
                        for (const auto& v : mesh->getVertices()) settings.mPoints.push_back(toJPH(v.position * tf.scale));
                    }
                    shape = settings.Create().Get();
                } else {
                    JPH::VertexList vertices;
                    JPH::IndexedTriangleList triangles;
                    uint32_t vertexBase = 0;
                    for (const auto& mesh : targetMeshes) {
                        for (const auto& v : mesh->getVertices()) vertices.push_back(toJPHFloat3(v.position * tf.scale));
                        const auto& indices = mesh->getIndices();
                        for (size_t i = 0; i < indices.size(); i += 3) {
                            triangles.push_back({indices[i] + vertexBase, indices[i+1] + vertexBase, indices[i+2] + vertexBase});
                        }
                        vertexBase += (uint32_t)mesh->getVertices().size();
                    }
                    shape = JPH::MeshShapeSettings(vertices, triangles).Create().Get();
                }
            }
        }

        if (!shape) shape = new JPH::BoxShape(toJPH(tf.scale * 0.5f));

        JPH::EMotionType motion = JPH::EMotionType::Static;
        JPH::ObjectLayer layer = Layers::NON_MOVING;
        if (phys.type == BodyType::Dynamic) { motion = JPH::EMotionType::Dynamic; layer = Layers::MOVING; }
        else if (phys.type == BodyType::Kinematic) { motion = JPH::EMotionType::Kinematic; layer = Layers::MOVING; }

        JPH::BodyCreationSettings settings(shape, toJPH(tf.translation), toJPH(glm::quat(tf.rotation)), motion, layer);
        settings.mFriction = phys.friction;
        settings.mRestitution = phys.restitution;
        settings.mLinearVelocity = toJPH(phys.initialLinearVelocity);
        settings.mMassPropertiesOverride.mMass = phys.mass;
        settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        settings.mLinearDamping = phys.linearDamping;
        settings.mAngularDamping = phys.angularDamping;

        // Enable CCD (Continuous Collision Detection) for dynamic bodies
        // Prevents tunneling through thin colliders (e.g. procedural planet mesh)
        if (phys.type == BodyType::Dynamic) {
            settings.mMotionQuality = JPH::EMotionQuality::LinearCast;
        }
        
        if (phys.constrain2D) {
            settings.mAllowedDOFs = JPH::EAllowedDOFs::Plane2D;
        }

        JPH::Body* body = bodyInterface.CreateBody(settings);
        bodyInterface.AddBody(body->GetID(), JPH::EActivation::Activate);
        phys.bodyID = body->GetID().GetIndexAndSequenceNumber();
    }

    void PhysicsWorld::destroyRigidBody(Entity entity) {
        if (!m_impl->initialized || !entity.has<PhysicsComponent>()) return;

        auto& phys = entity.get<PhysicsComponent>();
        if (phys.bodyID == 0xFFFFFFFF) return;

        auto& bodyInterface = m_impl->physicsSystem->GetBodyInterface();
        JPH::BodyID id(phys.bodyID);

        bodyInterface.RemoveBody(id);
        bodyInterface.DestroyBody(id);
        phys.bodyID = 0xFFFFFFFF;
    }

    void PhysicsWorld::createCharacterController(Entity entity) {
        if (!m_impl->initialized || !entity.has<CharacterControllerComponent>()) return;

        auto& tf = entity.get<TransformComponent>();
        auto& cc = entity.get<CharacterControllerComponent>();

        JPH::ShapeRefC shape;
        if (entity.has<PhysicsComponent>() && entity.get<PhysicsComponent>().colliderType == ColliderType::Capsule) {
            auto& phys = entity.get<PhysicsComponent>();
            shape = new JPH::CapsuleShape(phys.height * 0.5f * tf.scale.y, phys.radius * glm::max(tf.scale.x, tf.scale.z));
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

    void PhysicsWorld::clear() {
        if (!m_impl || !m_impl->initialized) return;

        auto& bodyInterface = m_impl->physicsSystem->GetBodyInterface();
        
        // Remove all bodies
        JPH::BodyIDVector allBodies;
        m_impl->physicsSystem->GetBodies(allBodies);
        
        if (!allBodies.empty()) {
            bodyInterface.RemoveBodies(allBodies.data(), (int)allBodies.size());
            bodyInterface.DestroyBodies(allBodies.data(), (int)allBodies.size());
        }

        // Remove all characters (JPH::Ref handles ref-counting cleanup)
        m_impl->characters.clear();

        BB_CORE_INFO("PhysicsWorld: All bodies and characters destroyed.");
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