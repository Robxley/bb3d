#include "bb3d/core/Engine.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/scene/OrbitCamera.hpp"
#include "bb3d/render/MeshGenerator.hpp"
#include "bb3d/core/Log.hpp"

int main() {
    // 1. Initialisation du moteur avec config fluide
    bb3d::EngineConfig config;
    config.window.setTitle("BB3D - Hello World Simplified")
                 .setResolution(1280, 720);
    
    auto engine = bb3d::Engine::Create(config);

    // 2. Création de la scène
    auto scene = engine->CreateScene();
    engine->SetActiveScene(scene);

    // 3. Ajout d'une caméra (Composant)
    auto cameraEntity = scene->createEntity("MainCamera");
    auto orbitCam = bb3d::CreateRef<bb3d::OrbitCamera>(45.0f, 1280.0f/720.0f, 0.1f, 100.0f);
    orbitCam->setTarget({0, 0, 0});
    orbitCam->rotate(0.0f, 300.0f); // Pitch initial de 30°
    cameraEntity.add<bb3d::CameraComponent>(orbitCam);

    // Ajout d'un script de contrôle (Souris Clic Gauche pour tourner, Molette pour Zoomer)
    cameraEntity.add<bb3d::NativeScriptComponent>([eng = engine.get()](bb3d::Entity entity, float dt) {
        auto& camComp = entity.get<bb3d::CameraComponent>();
        auto* orbit = dynamic_cast<bb3d::OrbitCamera*>(camComp.camera.get());
        if (!orbit) return;

        auto& input = eng->input();
        
        // Rotation à la souris (Clic Gauche maintenu)
        if (input.isMouseButtonPressed(bb3d::Mouse::Left)) {
            glm::vec2 delta = input.getMouseDelta();
            if (glm::length(delta) > 0.0f) {
                orbit->rotate(delta.x * 5.0f, -delta.y * 5.0f);
            }
        }

        // Zoom à la molette
        glm::vec2 scroll = input.getMouseScroll();
        if (std::abs(scroll.y) > 0.0f) {
            orbit->zoom(scroll.y);
        }

        // Contrôle au clavier (Flèches) comme alternative
        float speed = 150.0f * dt;
        if (input.isKeyPressed(bb3d::Key::Left)) orbit->rotate(-speed, 0);
        if (input.isKeyPressed(bb3d::Key::Right)) orbit->rotate(speed, 0);
        if (input.isKeyPressed(bb3d::Key::Up)) orbit->rotate(0, speed);
        if (input.isKeyPressed(bb3d::Key::Down)) orbit->rotate(0, -speed);
        
        // Zoom Clavier (PageUp/Down)
        if (input.isKeyPressed(bb3d::Key::PageUp)) orbit->zoom(5.0f * dt);
        if (input.isKeyPressed(bb3d::Key::PageDown)) orbit->zoom(-5.0f * dt);
    });

    // 4. Ajout d'objets (API Fluide)
    auto cubeMesh = bb3d::Ref<bb3d::Mesh>(bb3d::MeshGenerator::createCube(engine->graphics(), 1.0f, {1, 0, 0}).release());
    
    scene->createEntity("RedCube")
        .at({0, 0.5f, 0})
        .add<bb3d::MeshComponent>(cubeMesh);

    auto floorMesh = bb3d::Ref<bb3d::Mesh>(bb3d::MeshGenerator::createCheckerboardPlane(engine->graphics(), 20.0f, 20).release());
    scene->createEntity("Floor")
        .add<bb3d::MeshComponent>(floorMesh);

    BB_CORE_INFO("Hello World prêt ! CLIC GAUCHE pour tourner, MOLETTE pour zoomer.");

    // 5. Lancement automatique (Update + Render internes)
    engine->Run();

    return 0;
}
