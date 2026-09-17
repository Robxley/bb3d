# Plan d'implémentation de AstroBazard (Kerbal Space 2D)

**Objectif :** Créer un mini-jeu spatial 2D avec rendu 3D où le vaisseau conserve une taille apparente fixe tandis que la caméra s'éloigne pour révéler la planète complète.

**Architecture :** 
Nous allons réorganiser le dossier `apps/` en sous-dossiers pour chaque application. Ensuite, nous créerons un `main.cpp` spécifique pour `astro_bazard` exploitant les composants existants du moteur (Jolt Physics en 2D, Particules, SkySphere, NativeScriptComponent).
La conservation de la taille apparente du vaisseau sera gérée dans la boucle de mise à jour : la distance Z de la caméra augmente avec l'altitude, et le `scale` visuel du vaisseau est multiplié proportionnellement à cette distance Z (sans altérer son collider physique).

**Stack Technique :** C++20, Jolt Physics (contraintes 2D), spdlog, bb3d Engine Components.

---

### Tâche 1: Restructuration du dossier des applications

**Fichiers concernés :**
- Déplacer/Créer : `apps/bb3d_editor/main.cpp` (à partir de `apps/bb3d_editor.cpp`)
- Déplacer/Créer : `apps/bb3d_hello_world/main.cpp` (à partir de `apps/bb3d_hello_world.cpp`)
- Modifier : `apps/CMakeLists.txt`

**Étape 1 : Modification du CMakeLists.txt**
```cmake
# Remplacer le contenu de apps/CMakeLists.txt par :
add_executable(bb3d_hello_world bb3d_hello_world/main.cpp)
target_link_libraries(bb3d_hello_world PRIVATE bb3d_core)

if(BB3D_ENABLE_EDITOR)
    add_executable(bb3d_editor bb3d_editor/main.cpp)
    target_link_libraries(bb3d_editor PRIVATE bb3d_core)
endif()

add_executable(astro_bazard astro_bazard/main.cpp)
target_link_libraries(astro_bazard PRIVATE bb3d_core)
```

**Étape 2 : Lancer le build pour vérifier l'échec (avant déplacement des fichiers)**
Commande : `cmake --build build`
Attendu : Échec car CMake ne trouve pas les sous-dossiers et fichiers `main.cpp`.

**Étape 3 : Implémentation Minimale (Déplacement des fichiers)**
```bash
# Commandes shell pour déplacer les fichiers
mkdir -p apps/bb3d_editor
mkdir -p apps/bb3d_hello_world
mkdir -p apps/astro_bazard

mv apps/bb3d_editor.cpp apps/bb3d_editor/main.cpp
mv apps/bb3d_hello_world.cpp apps/bb3d_hello_world/main.cpp

# Fichier apps/astro_bazard/main.cpp basique
#include "bb3d/core/Engine.hpp"
int main(int argc, char** argv) { return 0; }
```

**Étape 4 : Lancer le build pour vérifier le succès**
Commande : `cmake .. -B build && cmake --build build`
Attendu : Succès (PASS), toutes les apps compilent.

**Étape 5 : Commit**
```bash
git rm apps/bb3d_editor.cpp apps/bb3d_hello_world.cpp
git add apps/CMakeLists.txt apps/bb3d_editor/main.cpp apps/bb3d_hello_world/main.cpp apps/astro_bazard/main.cpp
git commit -m "refactor(apps): réorganisation en sous-dossiers"
```

---

### Tâche 2: Squelette du Jeu AstroBazard et Scène de Base

**Fichiers concernés :**
- Modifier : `apps/astro_bazard/main.cpp`

**Étape 1 : Écrire le test qui échoue**
Aucun test CTest ici, l'objectif est d'avoir une application qui s'ouvre avec une scène valide affichant une sphère (planète) géante et une skybox cosmique.

**Étape 2 : Lancer l'app**
Commande : `apps/astro_bazard.exe` (ou via CTest si existant).
Attendu : Écran noir.

**Étape 3 : Implémentation Minimale**
```cpp
// Dans apps/astro_bazard/main.cpp
#include "bb3d/core/Engine.hpp"
#include "bb3d/scene/Scene.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/render/MeshGenerator.hpp"
#include <iostream>

using namespace bb3d;

int main(int argc, char** argv) {
    try {
        Engine::Get().Init();
        auto scene = CreateRef<Scene>(Engine::Get().graphics());
        
        // Planète
        auto planet = scene->createEntity("Planet");
        planet.add<MeshComponent>().mesh = MeshGenerator::createSphere(Engine::Get().graphics(), 1000.0f, 64, {0.1f, 0.4f, 0.8f});
        planet.get<TransformComponent>().translation = {0.0f, -1000.0f, 0.0f};

        // Environment
        scene->createSkybox("Cosmos", "assets/textures/skybox_stars.hdr"); // (Ou SkySphere générique)
        
        auto light = scene->createEntity("Sun");
        light.add<LightComponent>();
        light.get<TransformComponent>().rotation = {0.8f, -0.6f, 0.0f};

        Engine::Get().loadScene(scene);
        Engine::Get().Run();
        Engine::Get().Cleanup();
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
```

**Étape 4 : Build et Lancement**
Commande : `cmake --build build --target astro_bazard`
Attendu : Succès, l'application se lance avec une immense sphère bleue en fond.

**Étape 5 : Commit**
```bash
git add apps/astro_bazard/main.cpp
git commit -m "feat(astro): ajout de la scène planétaire de base"
```

---

### Tâche 3: Intégration du Vaisseau et de la Mécanique de Taille Constante

**Fichiers concernés :**
- Modifier : `apps/astro_bazard/main.cpp`

**Étape 1 : Implémentation de la logique de script et de la caméra**
Nous allons attacher un comportement asynchrone via `NativeScriptComponent` ou directement dans la boucle update de l'Engine avant `Run()` pour contrôler la caméra.

**Étape 3 : Implémentation Minimale**
```cpp
// Ajout dans le main() avant le Engine::Get().Run() :

auto ship = scene->createEntity("Spaceship");
ship.add<MeshComponent>().mesh = MeshGenerator::createCube(Engine::Get().graphics(), 2.0f, {0.9f, 0.9f, 0.9f});
ship.get<TransformComponent>().translation = {0.0f, 2.0f, 0.0f};

// Physique Jolt 2D
auto& phys = ship.add<PhysicsComponent>();
phys.type = BodyType::Dynamic;
phys.colliderType = ColliderType::Box;
phys.boxHalfExtents = {1.0f, 1.0f, 1.0f};
phys.mass = 10.0f;

auto camera = scene->createEntity("MainCamera");
auto& camComp = camera.add<CameraComponent>();
camComp.active = true;
camComp.farPlane = 10000.0f; // Très loin pour voir la planète

// Script comportemental pour le navire
struct RocketScript : public ScriptableBehaviour {
    Entity cam;
    float baseZoom = 20.0f;
    void onUpdate(float dt) override {
        // Entrées pour propulser
        auto& tf = getEntity().get<TransformComponent>();
        auto context = getEntity().getScene().getEngineContext();
        float inputPitch = 0.0f;
        // ... (lecture InputManager, application forces)
        
        // Logique de caméra (Zoom proportionnel à l'altitude)
        float altitude = tf.translation.y;
        float targetZoom = baseZoom + (altitude * 0.5f); // La caméra recule
        
        auto& camTf = cam.get<TransformComponent>();
        camTf.translation = { tf.translation.x, tf.translation.y, targetZoom };
        
        // Illusion de taille fixe : scale le mesh visuel
        // Scale est proportionnel à la distance Z de la caméra
        float scaleFactor = targetZoom / baseZoom;
        tf.scale = { scaleFactor, scaleFactor, scaleFactor };
    }
};

auto& script = ship.add<NativeScriptComponent>();
script.bind<RocketScript>();
script.instance->cam = camera; // Inutile de caster si l'Entity est public ou stockée proprement, 
// nous utiliserons une lambda enregistrée dans l'Engine::Get().addUpdateCallback() à la place car c'est plus direct.
```
*(Note détaillée du code complet dans l'implémentation finale de l'étape 3).*

**Étape 4 : Build et Lancement**
Commande : `cmake --build build --target astro_bazard`
Attendu : Le vaisseau reste à une taille constante visuellement tandis que le monde recule, la gravité opère et on peut pousser le cube (fusée) vers le haut.

**Étape 5 : Commit**
```bash
git add apps/astro_bazard/main.cpp
git commit -m "feat(astro): mécanique de taille constante et physique rudimentaire"
```
