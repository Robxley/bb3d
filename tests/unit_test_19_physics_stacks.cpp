#include "bb3d/core/Engine.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/render/MeshGenerator.hpp"
#include "bb3d/render/Material.hpp"
#include "bb3d/core/Log.hpp"
#include "bb3d/physics/PhysicsWorld.hpp"
#include <random>

using namespace bb3d;

Ref<PBRMaterial> loadMaterial(Engine& engine, const std::string& folder, const std::string& prefix) {
    auto mat = CreateRef<PBRMaterial>(engine.graphics());
    auto& assets = engine.assets();
    std::string path = "assets/PBR/" + folder + "/" + prefix;
    
    try {
        mat->setAlbedoMap(assets.load<Texture>(path + "_Color.jpg", true));
        mat->setNormalMap(assets.load<Texture>(path + "_NormalGL.jpg", false));
        mat->setORMMap(assets.load<Texture>(path + "_Roughness.jpg", false)); 
    } catch(...) {
        BB_CORE_WARN("Failed to load some textures for {0}", prefix);
    }
    return mat;
}

int main() {
    auto engine = Engine::Create(EngineConfig()
        .title("BB3D - Physics Shooter (Jolt)")
        .resolution(1600, 900)
        .vsync(true)
        .enablePhysics(PhysicsBackend::Jolt)
    );

    auto scene = engine->CreateScene();
    engine->SetActiveScene(scene);

    // 1. Setup Camera (FPS)
    auto player = scene->createFPSCamera("Player", 60.0f, 1600.0f/900.0f, {0, 5, 15}, engine.get());
    player->movementSpeed = {10.0f, 10.0f, 10.0f};

    // 2. Light
    scene->createDirectionalLight("Sun", {1, 0.95f, 0.8f}, 2.5f, {-45, 45, 0});
    scene->createSkySphere("Sky", "assets/textures/skybox_sphere_wood_diffuse.jpeg");

    // 3. Materials
    auto matFloor = loadMaterial(*engine, "Tiles008", "Tiles008_1K-JPG");
    auto matBox   = loadMaterial(*engine, "Bricks092_1K-JPG", "Bricks092_1K-JPG");
    
    auto matBall  = CreateRef<PBRMaterial>(engine->graphics());
    PBRParameters ballParams; 
    ballParams.baseColorFactor = {0.2f, 0.2f, 0.8f, 1.0f};
    ballParams.metallicFactor = 0.8f;
    ballParams.roughnessFactor = 0.2f;
    matBall->setParameters(ballParams);

    // 4. Ground
    auto ground = scene->createEntity("Ground");
    ground.at({0, -1, 0});
    auto groundMesh = Ref<Mesh>(MeshGenerator::createCheckerboardPlane(engine->graphics(), 100.0f, 20).release());
    groundMesh->setMaterial(matFloor);
    ground.add<MeshComponent>(groundMesh);
    
    auto& groundRB = ground.add<RigidBodyComponent>().get<RigidBodyComponent>();
    groundRB.type = BodyType::Static;
    
    ground.add<BoxColliderComponent>(glm::vec3(50.0f, 0.1f, 50.0f));
    engine->physics().createRigidBody(ground);

    // 5. Pyramid
    auto cubeMesh = Ref<Mesh>(MeshGenerator::createCube(engine->graphics(), 1.0f).release());
    cubeMesh->setMaterial(matBox);

    int baseSize = 8;
    float boxSize = 1.0f;
    float spacing = 0.05f;

    for (int y = 0; y < baseSize; ++y) {
        float offset = y * (boxSize + spacing) * 0.5f;
        for (int z = 0; z < (baseSize - y); ++z) {
            for (int x = 0; x < (baseSize - y); ++x) {
                auto cube = scene->createEntity("Box");
                float posX = (x * (boxSize + spacing)) + offset - (baseSize * boxSize * 0.5f);
                float posY = (boxSize * 0.5f) + y * (boxSize + spacing);
                float posZ = (z * (boxSize + spacing)) + offset - (baseSize * boxSize * 0.5f);
                
                cube.at({posX, posY, posZ});
                cube.add<MeshComponent>(cubeMesh);
                
                auto& rb = cube.add<RigidBodyComponent>().get<RigidBodyComponent>();
                rb.type = BodyType::Dynamic;
                rb.mass = 1.0f;
                rb.friction = 0.6f;

                cube.add<BoxColliderComponent>(glm::vec3(0.5f));
                engine->physics().createRigidBody(cube);
            }
        }
    }

    // 6. Projectiles meshes
    std::vector<Ref<Mesh>> ballMeshes;
    for(int i=0; i<5; i++) {
        float s = 0.2f + i * 0.1f;
        auto m = Ref<Mesh>(MeshGenerator::createSphere(engine->graphics(), s, 32).release());
        m->setMaterial(matBall);
        ballMeshes.push_back(m);
    }

    player.toEntity().add<NativeScriptComponent>([pScene = scene.get(), pEngine = engine.get(), ballMeshes](Entity e, float /*dt*/) {
        auto& input = pEngine->input();
        static bool lastClick = false;
        bool click = input.isMouseButtonPressed(Mouse::Left);

        if (click && !lastClick) {
            auto& tf = e.get<TransformComponent>();
            auto& ctrl = e.get<FPSControllerComponent>();

            // Calcul de la direction réelle de la caméra
            glm::vec3 front;
            front.x = cos(glm::radians(ctrl.yaw)) * cos(glm::radians(ctrl.pitch));
            front.y = sin(glm::radians(ctrl.pitch));
            front.z = sin(glm::radians(ctrl.yaw)) * cos(glm::radians(ctrl.pitch));
            glm::vec3 direction = glm::normalize(front);

            glm::vec3 spawnPos = tf.translation + direction * 1.5f;
            
            int meshIdx = rand() % ballMeshes.size();
            float radius = 0.2f + meshIdx * 0.1f;
            float mass = radius * 15.0f; 

            auto ball = pScene->createEntity("Projectile");
            ball.at(spawnPos);
            ball.add<MeshComponent>(ballMeshes[meshIdx]);
            
            auto& rb = ball.add<RigidBodyComponent>().get<RigidBodyComponent>();
            rb.type = BodyType::Dynamic;
            rb.mass = mass;
            rb.initialLinearVelocity = direction * 40.0f; // Propulsé dans l'axe de la vue !

            ball.add<SphereColliderComponent>(radius);
            
            pEngine->physics().createRigidBody(ball);
            BB_CORE_INFO("Pew! Ball spawned (Mass: {0})", mass);
        }
        lastClick = click;
    });

    BB_CORE_INFO("Physics Shooter Ready. Move with ZQSD, Click to shoot.");

    engine->Run();

    // Cleanup local references and scripts before shutdown
    scene->getRegistry().clear<NativeScriptComponent>();
    ballMeshes.clear();
    matFloor.reset();
    matBox.reset();
    matBall.reset();
    groundMesh.reset();
    cubeMesh.reset();
    scene.reset();

    engine->Shutdown();

    return 0;
}
