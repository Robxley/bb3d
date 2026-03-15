# Plan d'Implémentation : Refactorisation Modulaire AstroBazard

## Objectif
Extraire la gigantesque lambda de `main.cpp` vers des classes C++ réutilisables au sein de l'application `astro_bazard`. Cela rendra le code plus lisible et pérenne.

## Phase 1 : Structures de Base et Fichiers
- **Fichiers cibles** : Créer dans `apps/astro_bazard/`
  1. `SpaceshipController.hpp` et `.cpp` : Gestion des Inputs et Forces de propulsion.
  2. `OrbitalGravity.hpp` et `.cpp` : Application de $F = GM/d^2$ vers une entité cible (planète).
  3. `SmartCamera.hpp` et `.cpp` : Logique de tracking, recul selon l'altitude, et rotation "down".

## Phase 2 : Implémentation des Modules
- **Etape 1 : `OrbitalGravity`**
  - Constructeur : Prend l'Entité `planet` (la cible attractive) et la magnitude de base (ex: 500'000).
  - Méthode `update(Entity e, Engine* engine)` : Calcule le vecteur gravité pure et appelle `engine->physics().addForce()`.

- **Etape 2 : `SpaceshipController`**
  - Constructeur : Prend l'Entité `nose` (pour le positionner visuellement), paramètres de thrust/torque.
  - Méthode `update(Entity e, float dt, Engine* engine)` : Lit les inputs clavier, applique le `addForce` et `addTorque`, et met à jour le `TransformComponent` du nez de la fusée (incluant le scale adaptatif).

- **Etape 3 : `SmartCamera`**
  - Constructeur : Prend l'Entité `camera` et l'Entité `planet` (pour connaitre le vecteur Bas).
  - Méthode `update(Entity ship, float dt)` : Calcule l'altitude, ajuste le zoom Z, et fait pivoter la vue de la caméra en appelant explicitement `setViewMatrix` comme validé précédemment.

## Phase 3 : Intégration
- **Etape 4 : Refactorisation de `main.cpp`**
  - Instancier ces 3 objets locaux au dessus du script `NativeScriptComponent`.
  - Appeler leur `.update()` respectif à l'intérieur de la lambda allégée.
  - Ajouter les `.cpp` créés au `CMakeLists.txt` de l'application `astro_bazard` (via globbing automatique ou ciblé).

## Phase 4 : Compilation et Test
- Compiler l'exécutable `astro_bazard.exe` avec les nouveaux fichiers liés.
- Vérifier que les comportements (gravité, contrôles, caméra) sont strictement identiques.
