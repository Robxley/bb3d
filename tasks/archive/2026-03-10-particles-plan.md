# Plan d'implémentation de Particle System (bb3d)

**Objectif :** Créer un système de particules performant (2D instancié et Mesh 3D) avec option d'injection Jolt Physics, puis l'utiliser pour la tuyère de la fusée AstroBazard.

**Architecture :** Composant `ParticleSystemComponent` qui gère un pool de struct `Particle`. Le rendu est effectué en instanciation via `Renderer` (comme pour les modèles 3D multiples). Un script AstroBazard lie l'émission de la fusée avec les touches espace/z/q/d.

**Stack Technique :** C++20, Vulkan (SSBO Instancing), Jolt Physics (pour le mode dynamique facultatif), entt (ECS).

---

### Tâche 1: Création du composant cible `ParticleSystemComponent`

**Fichiers concernés :**
- Modifier : `c:\dev\bb3d\include\bb3d\scene\Components.hpp`
- Modifier : `c:\dev\bb3d\src\bb3d\scene\SceneSerializer.cpp`

**Étape 1 : Implémentation Minimale**
Ajouter la struct `Particle` et `ParticleSystemComponent` dans `Components.hpp` incluant le pool vectoriel, la configuration d'émission, et les méthodes de sérialisation. 

**Étape 2 : Commit**
```bash
git add include/bb3d/scene/Components.hpp src/bb3d/scene/SceneSerializer.cpp
git commit -m "feat: ajout de ParticleSystemComponent et sérialisation"
```

---

### Tâche 2: Intégration dans Scene.cpp (Mise à jour Cycle de Vie & Physique)

**Fichiers concernés :**
- Modifier : `c:\dev\bb3d\src\bb3d\scene\Scene.cpp`
- Modifier : `c:\dev\bb3d\include\bb3d\scene\Scene.hpp`

**Étape 1 : Implémentation Minimale**
Dans `Scene::update`, ajouter une vue sur les `ParticleSystemComponent`. Boucler sur les particules actives pour réduire leur `lifeRemaining`, mettre à jour leur `position` basée sur `velocity * dt`. Gérer la création d'entités Jolt Physics à la volée  si `injectIntoPhysics` est activé, et synchroniser la `position` de la particule depuis Jolt.

**Étape 2 : Lancer la compilation**
Commande : `cmake --build build --config Debug`

---

### Tâche 3: Intégration Rendu SSBO Instancié (Renderer)

**Fichiers concernés :**
- Modifier : `c:\dev\bb3d\include\bb3d\render\Renderer.hpp`
- Modifier : `c:\dev\bb3d\src\bb3d\render\Renderer.cpp`

**Étape 1 : Implémentation Minimale**
Le `Renderer::render()` va scanner les `ParticleSystemComponent`. Pour les particules actives, on construit un Array de `glm::mat4` et on les soumet au pipeline Vulkan via le même SSBO d'instanciation déjà fonctionnel. 

**Étape 2 : Lancer la compilation**

---

### Tâche 4: Intégration Gameplay AstroBazard

**Fichiers concernés :**
- Modifier : `c:\dev\bb3d\apps\astro_bazard\SpaceshipController.cpp`
- Modifier : `c:\dev\bb3d\apps\astro_bazard\main.cpp`

**Étape 1 : Implémentation Minimale**
Instancier le composant de particules sous le vaisseau.
Dans `SpaceshipController::update`, ajouter l'émission `.emit()` associée aux touches de pression `W/Up` (propulsion arrière) et `A/D` (RCS). L'émission calculera une vélocité opposée à la poussée spatiale de la fusée.

**Étape 2 : Lancer la compilation et Test Run**
Commande : `cmake --build build --config Debug && build\bin\astro_bazard.exe`
Attendu : Le jeu se lance et des particules sortent de la tige en appuyant sur Espace/Z/Q/D.
