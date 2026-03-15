# Architecture Modulaire - AstroBazard (Option A)

## Objectif
Permettre la pérennité et l'évolutivité du jeu `AstroBazard` en séparant la logique contenue actuellement dans `main.cpp` vers des modules orientés "Métier" propres au jeu, tout en conservant le code de `bb3d` intact.

## Structure de Fichiers (Dossier : `apps/astro_bazard/`)
Les fichiers suivants seront créés au sein de l'application AstroBazard pour modéliser le jeu :
- `SpaceshipController.hpp / .cpp` : Gestion des entrées utilisateur (clavier) et application des forces physiques (propulsion et couple/rotation).
- `OrbitalGravity.hpp / .cpp` : Système de gravité de Newton ($F = GM/d^2$) appliquant une force gravitationnelle pure sur un corps cible vers un corps attracteur (la planète).
- `SmartCamera.hpp / .cpp` : Script assujettissant la caméra à l'orientation gravitationnelle (l'axe "Bas" de l'écran pointe toujours vers la planète) et un recul adaptatif en Z en fonction de l'altitude.

## Interactions et Flow
1. **Initialisation** : `main.cpp` créera les entités via `bb3d::Engine`.
2. Au lieu de définir des lambdas lourdes (`NativeScriptComponent`), on attachera nos classes personnalisées via des fonctions comme `SpaceshipController::bind(Entity ship)`.
3. À chaque _Update_, ces scripts locaux mettront à jour la physique (Jolt) ou la matrice de vue (`Camera::setViewMatrix`).

## Validation
Ce design "Option A" isole complètement les logiques orbitale spatiales de la base de code générique du moteur Vulkan. Le système de scripting du moteur via `NativeScriptComponent` sera utilisé pour injecter nos nouvelles briques C++ propres au jeu.
