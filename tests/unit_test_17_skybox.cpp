#include "bb3d/core/Engine.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/scene/OrbitCamera.hpp"
#include "bb3d/render/MeshGenerator.hpp"
#include "bb3d/render/Material.hpp"
#include "bb3d/core/Log.hpp"

int main() {
    auto engine = bb3d::Engine::Create(bb3d::EngineConfig()
        .title("BB3D - SkySphere Final Test")
        .resolution(1280, 720)
        .vsync(true)
    );

    auto scene = engine->CreateScene();
    engine->SetActiveScene(scene);

    auto cameraEntity = scene->createEntity("MainCamera");
    auto orbitCam = bb3d::CreateRef<bb3d::OrbitCamera>(45.0f, 1280.0f/720.0f, 0.1f, 1000.0f);
    orbitCam->setTarget({0, 0, 0});
    orbitCam->zoom(-5.0f);
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
    });

    auto& assets = engine->assets();

    // 1. SkySphere
    auto skySphere = scene->createEntity("SkyEnvironment");
    try {
        auto tex = assets.load<bb3d::Texture>("assets/textures/skybox_sphere_wood_diffuse.jpeg", true);
        skySphere.add<bb3d::SkySphereComponent>(tex);
    } catch (...) {}

    // 2. Sphère Rouge Explicite
    auto sphereMesh = bb3d::Ref<bb3d::Mesh>(bb3d::MeshGenerator::createSphere(engine->graphics(), 1.0f, 64, {1,1,1}).release());
    auto redMat = bb3d::CreateRef<bb3d::PBRMaterial>(engine->graphics());
    bb3d::PBRParameters params;
    params.baseColorFactor = {1.0f, 0.0f, 0.0f, 1.0f}; // ROUGE
    redMat->setParameters(params);
    sphereMesh->setMaterial(redMat);

    scene->createEntity("RedSphere")
        .at({0, 0, 0})
        .add<bb3d::MeshComponent>(sphereMesh, "", bb3d::PrimitiveType::Sphere);

    // 3. Lumière
    scene->createEntity("Sun").add<bb3d::LightComponent>(bb3d::LightType::Directional, glm::vec3(1,1,1), 10.0f);

    engine->Run();

    scene->getRegistry().clear();
    engine->SetActiveScene(nullptr);
    scene.reset();
    
    sphereMesh.reset();
    redMat.reset();

    engine->Shutdown();
    bb3d::Material::Cleanup();

    return 0;
}
