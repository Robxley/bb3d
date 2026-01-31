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

## 🏗️ 2. Gestion de la Scène et des Entités (High-Level API)

L'API de la scène propose des méthodes utilitaires pour créer rapidement des objets complexes.

### Création d'une Caméra (Orbitale / FPS)
Ces méthodes créent l'entité, la caméra et le script de contrôle automatiquement.

```cpp
// Caméra Orbitale (Style Editeur/RTS) : Clic Gauche pour tourner, Molette pour zoomer
auto orbitCam = scene->createOrbitCamera("MainCam", 45.0f, 16.0f/9.0f, {0,0,0}, 10.0f);

// Caméra FPS (First Person) : ZQSD pour bouger, Clic Droit pour tourner
auto fpsCam = scene->createFPSCamera("PlayerCam", 60.0f, 16.0f/9.0f, {0,2,0});
```

### Chargement de Modèles 3D
Charge un modèle, crée l'entité et le redimensionne optionnellement.

```cpp
// Chargement simple
auto car = scene->createModelEntity("Car", "assets/models/car.glb", {0,0,0});

// Chargement avec normalisation de taille (utile si l'échelle du modèle est inconnue)
auto giantAnt = scene->createModelEntity("Ant", "assets/models/ant.glb", {0,2,-10}, {20.0f, 20.0f, 20.0f});
```

## 📦 3. Composants et Logique

### Ajouter une Lumière
```cpp
// Lumière Directionnelle (Soleil)
scene->createDirectionalLight("Sun", {1.0f, 0.9f, 0.8f}, 5.0f, {-45.0f, 0, 0});

// Lumière Ponctuelle (Lampe)
scene->createPointLight("Lamp", {1,0,0}, 10.0f, 20.0f, {0,5,0});
```

### Scripting Rapide (Native Script)
```cpp
entity.add<bb3d::NativeScriptComponent>([](bb3d::Entity ent, float dt) {
    auto& transform = ent.get<bb3d::TransformComponent>();
    transform.rotation.y += dt; // Rotation continue
});
```

## 🌍 4. Environnement

### SkySphere (Panorama 360°)
```cpp
// Création automatique (charge la texture et configure le composant)
scene->createSkySphere("Sky", "assets/textures/sky.jpg");
```

---

## 🛠️ 5. Advanced Snippets (Low-Level / Custom)

Cette section montre comment assembler manuellement des entités complexes si les méthodes automatiques ne suffisent pas (ex: Caméra Custom).

### Caméra Custom avec Script Manuel
Si vous voulez un comportement de caméra spécifique non couvert par Orbit/FPS.

```cpp
// 1. Création de l'entité et de la caméra (classe dérivée de Camera ou OrbitCamera)
auto cameraEntity = scene->createEntity("CustomCam");
auto myCam = bb3d::CreateRef<bb3d::OrbitCamera>(45.0f, 16.0f/9.0f, 0.1f, 1000.0f);
cameraEntity.add<bb3d::CameraComponent>(myCam);

// 2. Ajout du script de contrôle manuel
cameraEntity.add<bb3d::NativeScriptComponent>([eng = engine.get()](bb3d::Entity entity, float dt) {
    auto& camComp = entity.get<bb3d::CameraComponent>();
    auto* cam = dynamic_cast<bb3d::OrbitCamera*>(camComp.camera.get());
    if (!cam) return;

    auto& input = eng->input();
    
    // Exemple : Zoom avec les touches PageUp/PageDown au lieu de la molette
    if (input.isKeyPressed(bb3d::Key::PageUp))   cam->zoom(-10.0f * dt);
    if (input.isKeyPressed(bb3d::Key::PageDown)) cam->zoom( 10.0f * dt);
});
```

### Chargement Manuel de Modèle (Try/Catch)
Si vous avez besoin de gérer les erreurs de chargement spécifiquement.

```cpp
try {
    auto& assets = engine->assets();
    auto model = assets.load<bb3d::Model>("assets/models/complex_object.obj");
    
    // Manipulation du modèle avant création de l'entité
    model->normalize({1.0f, 1.0f, 1.0f});

    auto entity = scene->createEntity("ManualEntity");
    entity.at({0, 5, 0});
    entity.add<bb3d::ModelComponent>(model);
} catch (const std::exception& e) {
    BB_CORE_ERROR("Erreur lors du chargement manuel : {}", e.what());
}
```