#include "bb3d/core/Engine.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/scene/OrbitCamera.hpp"
#include "bb3d/render/MeshGenerator.hpp"
#include "bb3d/render/Material.hpp"
#include "bb3d/core/Log.hpp"

int main() {
    auto engine = bb3d::Engine::Create(bb3d::EngineConfig()
        .title("BB3D - PBR Test")
        .resolution(1280, 720)
        .vsync(true)
    );

    auto scene = engine->CreateScene();
    engine->SetActiveScene(scene);

    // Caméra
    auto cameraEntity = scene->createEntity("MainCamera");
    auto orbitCam = bb3d::CreateRef<bb3d::OrbitCamera>(45.0f, 1280.0f/720.0f, 0.1f, 100.0f);
    orbitCam->setTarget({0, 0, 0});
    orbitCam->zoom(-3.0f);
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

    // PBR Material
    auto material = bb3d::CreateRef<bb3d::Material>(engine->graphics());
    auto& assets = engine->assets();

    // Textures Bricks PBR
    const std::string baseDir = "assets/PBR/Bricks092_1K-JPG/";
    try {
        material->setAlbedoMap(assets.load<bb3d::Texture>(baseDir + "Bricks092_1K-JPG_Color.jpg"));
        material->setNormalMap(assets.load<bb3d::Texture>(baseDir + "Bricks092_1K-JPG_NormalGL.jpg"));
        
        // Bricks are non-metallic -> Default MetallicMap is black (0.0), which is correct.
        // We set the Roughness map explicitly.
        material->setRoughnessMap(assets.load<bb3d::Texture>(baseDir + "Bricks092_1K-JPG_Roughness.jpg"));
        
        material->setAOMap(assets.load<bb3d::Texture>(baseDir + "Bricks092_1K-JPG_AmbientOcclusion.jpg"));
        
        BB_CORE_INFO("Textures PBR Bricks chargées avec succès.");
    } catch (const std::exception& e) {
        BB_CORE_WARN("Erreur chargement textures PBR: {}", e.what());
    }

    // Sphère PBR
    auto sphereMesh = bb3d::Ref<bb3d::Mesh>(bb3d::MeshGenerator::createSphere(engine->graphics(), 1.0f, 64, {1,1,1}).release());
    sphereMesh->setMaterial(material);

    scene->createEntity("PBRSphere")
        .at({0, 0, 0})
        .add<bb3d::MeshComponent>(sphereMesh);

    BB_CORE_INFO("PBR Scene Ready. Left Click to Rotate.");

    engine->Run();

    return 0;
}