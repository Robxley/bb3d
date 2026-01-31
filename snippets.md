# 💡 biobazard3d - Code Snippets & Tips

Ce document regroupe les blocs de code les plus courants pour utiliser le moteur.

## 🚀 1. Initialisation de l'Engine

L'approche préconisée utilise l'API fluide (Builder pattern) pour configurer le moteur proprement.

```cpp
#include "bb3d/core/Engine.hpp"

int main() {
    // Configuration et création
    auto engine = bb3d::Engine::Create(bb3d::EngineConfig()
        .title("Ma Super Démo")
        .resolution(1920, 1080)
        .vsync(true)
        .enablePhysics(bb3d::PhysicsBackend::Jolt)
    );

    // Lancement de la boucle principale
    engine->Run();
    return 0;
}
```

## 🏗️ 2. Gestion de la Scène et des Entités

Le moteur utilise un système **ECS (Entity Component System)**.

```cpp
// Récupérer la scène active ou en créer une nouvelle
auto scene = engine->CreateScene();
engine->SetActiveScene(scene);

// Créer une entité
auto player = scene->createEntity("Player");

// Positionner une entité (API fluide)
player.at({0.0f, 1.0f, -5.0f});

// Ajouter un composant
player.add<bb3d::TagComponent>("Hero");
```

## 📦 3. Chargement des Assets (Ressources)

Le `ResourceManager` gère le cache et évite les doublons mémoire.

```cpp
auto& assets = engine->assets();

// Modèles (OBJ ou GLTF)
auto model = assets.load<bb3d::Model>("assets/models/car.glb");

// Textures
auto texture = assets.load<bb3d::Texture>("assets/textures/diffuse.png");

// Utilisation dans un composant
entity.add<bb3d::ModelComponent>(model);
```

## 🎥 4. Caméras (Standard & Scriptées)

### Caméra Orbitale (Style Blender/Editeur)
```cpp
auto cameraEntity = scene->createEntity("OrbitCam");
auto orbit = bb3d::CreateRef<bb3d::OrbitCamera>(45.0f, 16.0f/9.0f, 0.1f, 1000.0f);
orbit->setTarget({0, 0, 0});
orbit->setDistance(10.0f);

cameraEntity.add<bb3d::CameraComponent>(orbit);

// Script pour le contrôle à la souris
cameraEntity.add<bb3d::NativeScriptComponent>([eng = engine.get()](bb3d::Entity entity, float dt) {
    auto& cam = entity.get<bb3d::CameraComponent>();
    auto* orbit = dynamic_cast<bb3d::OrbitCamera*>(cam.camera.get());
    auto& input = eng->input();

    if (input.isMouseButtonPressed(bb3d::Mouse::Left)) {
        glm::vec2 delta = input.getMouseDelta();
        orbit->rotate(delta.x * 5.0f, -delta.y * 5.0f);
    }
    orbit->zoom(input.getMouseScroll().y);
});
```

### Caméra FPS
```cpp
auto fpsCam = bb3d::CreateRef<bb3d::FPSCamera>(60.0f, 16.0f/9.0f, 0.1f, 1000.0f);
entity.add<bb3d::CameraComponent>(fpsCam);
```

## 🌍 5. Environnement (Skybox & SkySphere)

### Skybox (Cubemap - 6 faces)
```cpp
// Requiert un asset au format cubemap (.ktx2 ou chargement spécifique)
// scene->setSkybox(myCubemapTexture);
```

### SkySphere (Texture 360° Panorama)
```cpp
auto skyTex = assets.load<bb3d::Texture>("assets/textures/sky.jpg");
scene->createEntity("Sky").add<bb3d::SkySphereComponent>(skyTex);
```

## 💡 6. Comportements Personnalisés (Native Scripts)

C'est le moyen le plus simple d'ajouter de la logique à une entité sans créer de classe complexe.

```cpp
entity.add<bb3d::NativeScriptComponent>([](bb3d::Entity ent, float dt) {
    // Récupérer le transform
    auto& transform = ent.get<bb3d::TransformComponent>();
    
    // Faire tourner l'objet
    transform.rotation.y += dt * glm::radians(45.0f);
});
```

## ☀️ 7. Éclairage

```cpp
auto light = scene->createEntity("Sun");
light.add<bb3d::LightComponent>(bb3d::LightType::Directional, glm::vec3(1.0f, 0.9f, 0.8f), 5.0f);
light.get<bb3d::TransformComponent>().rotation = {glm::radians(-45.0f), 0, 0};
```
