#include "bb3d/core/Engine.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/scene/OrbitCamera.hpp"
#include "bb3d/render/Material.hpp"
#include "bb3d/core/Log.hpp"
#include <random>

int main() {
    auto engine = bb3d::Engine::Create(bb3d::EngineConfig()
        .title("BB3D - Particle System Test")
        .resolution(1280, 720)
        .vsync(true)
        .enableOffscreenRendering(true) // Required for Editor Viewport
        .enableEditor(true) // Enable editor to see the properties
    );

    auto scene = engine->CreateScene();
    engine->SetActiveScene(scene);

    // 1. Camera
    auto cameraEntity = scene->createEntity("MainCamera");
    auto orbitCam = bb3d::CreateRef<bb3d::OrbitCamera>(45.0f, 1280.0f/720.0f, 0.1f, 100.0f);
    orbitCam->setTarget({0, 1, 0});
    orbitCam->zoom(-10.0f);
    cameraEntity.add<bb3d::CameraComponent>(orbitCam);

    cameraEntity.add<bb3d::NativeScriptComponent>([eng = engine.get()](bb3d::Entity entity, float dt) {
        auto& camComp = entity.get<bb3d::CameraComponent>();
        auto* orbit = dynamic_cast<bb3d::OrbitCamera*>(camComp.camera.get());
        if (!orbit) return;
        auto& input = eng->input();
        if (input.isMouseButtonPressed(bb3d::Mouse::Left)) {
            glm::vec2 delta = input.getMouseDelta();
            orbit->rotate(delta.x * 5.0f, -delta.y * 5.0f);
        }
        orbit->zoom(input.getMouseScroll().y);
    });

    // 2. Particle Material
    auto& assets = engine->assets();
    auto fireTex = assets.load<bb3d::Texture>("assets/textures/particles/PNG (Black background)/fire_01.png", true);
    
    auto particleMat = bb3d::CreateRef<bb3d::ParticleMaterial>(engine->graphics());
    particleMat->setBaseMap(fireTex);
    particleMat->setColor({1.0f, 0.5f, 0.2f}, 0.8f); // Orange fire color

    // 3. Particle Emitter Entity
    auto emitter = scene->createEntity("FireEmitter");
    emitter.at({0, 0, 0});
    
    emitter.add<bb3d::ParticleSystemComponent>(2000);
    auto& pSys = emitter.get<bb3d::ParticleSystemComponent>();
    pSys.material = particleMat;

    // 4. Emission Script
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

    emitter.add<bb3d::NativeScriptComponent>([dis, gen](bb3d::Entity entity, float dt) mutable {
        static float timer = 0.0f;
        timer += dt;
        
        // Emit 5 particles every 0.01s (approx 500/s)
        if (timer > 0.01f) {
            timer = 0.0f;
            auto& pSysComp = entity.get<bb3d::ParticleSystemComponent>();
            for(int i=0; i<5; ++i) {
                bb3d::ParticleProps props;
                props.position = entity.get<bb3d::TransformComponent>().translation;
                props.velocity = { dis(gen) * 0.5f, 2.0f + dis(gen) * 0.5f, dis(gen) * 0.5f };
                props.velocityVariation = { 0.2f, 0.2f, 0.2f };
                props.colorBegin = { 1.0f, 0.8f, 0.3f, 1.0f };
                props.colorEnd = { 1.0f, 0.1f, 0.0f, 0.0f }; // Fade to transparent red
                props.sizeBegin = 0.5f + dis(gen) * 0.1f;
                props.sizeEnd = 0.0f;
                props.sizeVariation = 0.1f;
                props.lifeTime = 1.5f + dis(gen) * 0.5f;
                
                pSysComp.emit(props);
            }
        }
    });

    // 5. Background Environment
    scene->createSkySphere("Sky", "assets/textures/horn-koppe_spring_4k.hdr", true); // flipY = true
    scene->createDirectionalLight("Sun", {1, 1, 1}, 1.0f, {-45, -45, 0});

    BB_CORE_INFO("Particle Test Ready. Texture: assets/textures/particles/fire.png");

    engine->Run();

    // Cleanup
    scene->getRegistry().clear();
    engine->SetActiveScene(nullptr);
    scene.reset();
    fireTex.reset();
    particleMat.reset();
    
    engine->Shutdown();
    bb3d::Material::Cleanup();

    return 0;
}
