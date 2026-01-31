#include "bb3d/core/Engine.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/scene/OrbitCamera.hpp"
#include "bb3d/render/MeshGenerator.hpp"
#include "bb3d/render/Material.hpp"
#include "bb3d/core/Log.hpp"

int main() {
    auto engine = bb3d::Engine::Create(bb3d::EngineConfig()
        .title("BB3D - Multi-Materials Test (PBR, Unlit, Toon)")
        .resolution(1280, 720)
        .vsync(true)
    );

    auto scene = engine->CreateScene();
    engine->SetActiveScene(scene);

    // Caméra
    auto cameraEntity = scene->createEntity("MainCamera");
    auto orbitCam = bb3d::CreateRef<bb3d::OrbitCamera>(45.0f, 1280.0f/720.0f, 0.1f, 100.0f);
    orbitCam->setTarget({0, 0, 0});
    orbitCam->zoom(-6.0f); // Reculer pour voir les 3 sphères
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

    auto& assets = engine->assets();
    const std::string baseDir = "assets/PBR/Bricks092_1K-JPG/";
    bb3d::Ref<bb3d::Texture> albedoTex = nullptr;
    bb3d::Ref<bb3d::Texture> normalTex = nullptr;
    bb3d::Ref<bb3d::Texture> roughTex = nullptr;
    bb3d::Ref<bb3d::Texture> aoTex = nullptr;

    try {
        if (std::filesystem::exists(baseDir + "Bricks092_1K-JPG_Color.jpg"))
            albedoTex = assets.load<bb3d::Texture>(baseDir + "Bricks092_1K-JPG_Color.jpg", true); // Color
        if (std::filesystem::exists(baseDir + "Bricks092_1K-JPG_NormalGL.jpg"))
            normalTex = assets.load<bb3d::Texture>(baseDir + "Bricks092_1K-JPG_NormalGL.jpg", false); // Data
        if (std::filesystem::exists(baseDir + "Bricks092_1K-JPG_Roughness.jpg"))
            roughTex = assets.load<bb3d::Texture>(baseDir + "Bricks092_1K-JPG_Roughness.jpg", false); // Data
        if (std::filesystem::exists(baseDir + "Bricks092_1K-JPG_AmbientOcclusion.jpg"))
            aoTex = assets.load<bb3d::Texture>(baseDir + "Bricks092_1K-JPG_AmbientOcclusion.jpg", false); // Data
    } catch (...) { BB_CORE_WARN("Textures non trouvées"); }

    // Script de rotation partagé
    auto rotationScript = [](bb3d::Entity entity, float dt) {
        auto& transform = entity.get<bb3d::TransformComponent>();
        transform.rotation.y += 1.0f * dt;
    };

    // --- 1. PBR Sphere (Centre) ---
    auto matPBR = bb3d::CreateRef<bb3d::PBRMaterial>(engine->graphics());
    matPBR->setAlbedoMap(albedoTex);
    matPBR->setNormalMap(normalTex);
    matPBR->setRoughnessMap(roughTex);
    matPBR->setAOMap(aoTex);

    auto meshPBR = bb3d::Ref<bb3d::Mesh>(bb3d::MeshGenerator::createSphere(engine->graphics(), 1.0f, 64, {1,1,1}).release());
    meshPBR->setMaterial(matPBR);
    scene->createEntity("SpherePBR")
        .at({0, 0, 0})
        .add<bb3d::MeshComponent>(meshPBR)
        .add<bb3d::NativeScriptComponent>(rotationScript);

    // --- 2. Unlit Sphere (Gauche) ---
    auto matUnlit = bb3d::CreateRef<bb3d::UnlitMaterial>(engine->graphics());
    matUnlit->setBaseMap(albedoTex); // Juste la couleur, pas d'ombrage

    auto meshUnlit = bb3d::Ref<bb3d::Mesh>(bb3d::MeshGenerator::createSphere(engine->graphics(), 1.0f, 64, {1,1,1}).release());
    meshUnlit->setMaterial(matUnlit);
    scene->createEntity("SphereUnlit")
        .at({-2.5f, 0, 0})
        .add<bb3d::MeshComponent>(meshUnlit)
        .add<bb3d::NativeScriptComponent>(rotationScript);

    // --- 3. Toon Sphere (Droite) ---
    auto matToon = bb3d::CreateRef<bb3d::ToonMaterial>(engine->graphics());
    matToon->setBaseMap(albedoTex); // Ombrage Cel-Shaded

    auto meshToon = bb3d::Ref<bb3d::Mesh>(bb3d::MeshGenerator::createSphere(engine->graphics(), 1.0f, 64, {1,1,1}).release());
    meshToon->setMaterial(matToon);
    scene->createEntity("SphereToon")
        .at({2.5f, 0, 0})
        .add<bb3d::MeshComponent>(meshToon)
        .add<bb3d::NativeScriptComponent>(rotationScript);

    BB_CORE_INFO("Scene Materials Ready: Left=Unlit, Center=PBR, Right=Toon");

    engine->Run();

    return 0;
}
