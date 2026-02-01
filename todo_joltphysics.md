# 🧱 Jolt Physics Integration - TODO List

Ce document suit l'intégration du moteur de physique Jolt dans biobazard3d (Phase 6.2).

## 1. Fondations (Infrastructure)
- [ ] **Ajouter Jolt au CMake** : Utiliser `FetchContent` pour intégrer Jolt Physics.
- [ ] **Initialisation de base** : Configurer les allocateurs, le Job System de Jolt et le `PhysicsSystem` dans `PhysicsWorld::init`.
- [ ] **Gestion des Couches (Layers)** : Définir les couches de collision (NonMoving, Moving) et le filtre de collision.
- [ ] **Step Simulation** : Implémenter la boucle `PhysicsWorld::update` avec un pas de temps fixe (Time Stepping).

## 2. Intégration ECS & Composants
- [ ] **RigidBodyComponent** : Étendre le composant existant pour stocker le `BodyID` de Jolt.
- [ ] **Colliders** : Implémenter la création de formes Jolt (Box, Sphere, Capsule) à partir des composants.
- [ ] **Synchronisation Transform** : 
    - [ ] `Jolt -> Engine` : Mettre à jour `TransformComponent` à partir de l'état Jolt (Autorité master).
    - [ ] `Engine -> Jolt` : Permettre la téléportation/modification manuelle du transform vers Jolt (Kinematic).

## 3. Fonctionnalités Avancées
- [ ] **Raycasting** : Ajouter une API pour lancer des rayons dans le monde physique.
- [ ] **Character Controller** : Intégrer le contrôleur de personnage de Jolt pour des déplacements fluides (escaliers, pentes).
- [ ] **Mesh Collider** : Pouvoir utiliser la géométrie des `bb3d::Mesh` comme collision statique.

## 4. Tests & Validation
- [ ] **unit_test_18_physics_basic** : Une boîte tombe sur un plan statique.
- [ ] **unit_test_19_physics_stacks** : Une pyramide de boîtes pour tester la stabilité.
- [ ] **Demo Integration** : Intégrer la physique dans la démo principale (avions qui tombent ?).
