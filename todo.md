# 📝 biobazard3d - TODO List

Ce document suit l'évolution du moteur biobazard3d. Les tâches terminées sont marquées pour garder une trace de la progression.

## ✅ Terminé (Archive des Features)
- [x] 🏗️ **Core Architecture** : Singleton Engine, Windowing (SDL3), Logging (spdlog), Profiling (Tracy).
- [x] 🎨 **Vulkan Backend** : Initialisation Vulkan 1.3, Dynamic Rendering (sans RenderPass legacy), VMA.
- [x] 📦 **Asset Loading** : Chargeur OBJ (tinyobjloader) et glTF 2.0 (fastgltf) avec support des matériaux.
- [x] 💎 **PBR Rendering** : Modèle Cook-Torrance complet (Albedo, Normal, ORM, Emissive).
- [x] ⚡ **GPU Instancing** : Batching automatique via SSBO pour dessiner des milliers d'objets en un seul Draw Call.
- [x] 💡 **Multi-Lights** : Support de 10 lumières simultanées (Directional & Point) avec atténuation physique.
- [x] ✨ **Cel-Shading** : Rendu cartoon avec quantification des couleurs et Outlines (contours).
- [x] 📂 **Serialization** : Système de sauvegarde/chargement de scène au format JSON.
- [x] 🧵 **JobSystem** : Thread Pool multi-coeur pour les tâches asynchrones.
- [x] 📐 **Maths & Camera** : Intégration GLM, Caméras FPS et Orbitale interactives.
- [x] 🧹 **Harmonisation Assets** : Standardisation des noms de fichiers et structures de dossiers pour les modèles.
- [x] 🪟 **Window Resizing** : Gestion robuste du redimensionnement et de la minimisation (Swapchain recreation).
- [x] 🧩 **ECS & View Architecture** : Refonte vers un ECS pur (Composants de contrôle séparés) et introduction de `View<T>` pour un accès typé sans overhead.
- [x] 🌍 **Intégration Jolt Physics** : Simulation réelle avec RigidBodies, Colliders, Raycasting et Character Controller.
    - **1. Fondations (Infrastructure)**
        - [x] **Ajouter Jolt au CMake** : Utiliser `FetchContent` pour intégrer Jolt Physics.
        - [x] **Initialisation de base** : Configurer les allocateurs, le Job System de Jolt et le `PhysicsSystem` dans `PhysicsWorld::init`.
        - [x] **Gestion des Couches (Layers)** : Définir les couches de collision (NonMoving, Moving) et le filtre de collision.
        - [x] **Step Simulation** : Implémenter la boucle `PhysicsWorld::update` avec un pas de temps fixe (Time Stepping).
    - **2. Intégration ECS & Composants**
        - [x] **RigidBodyComponent** : Étendre le composant existant pour stocker le `BodyID` de Jolt.
        - [x] **Colliders** : Implémenter la création de formes Jolt (Box, Sphere, Capsule) à partir des composants.
        - [x] **Synchronisation Transform** : 
            - [x] `Jolt -> Engine` : Mettre à jour `TransformComponent` à partir de l'état Jolt (Autorité master).
            - [x] `Engine -> Jolt` : Permettre la téléportation/modification manuelle du transform vers Jolt (Kinematic).
    - **3. Fonctionnalités Avancées**
        - [x] **Raycasting** : Ajouter une API pour lancer des rayons dans le monde physique.
        - [x] **Character Controller** : Intégrer le contrôleur de personnage de Jolt pour des déplacements fluides (escaliers, pentes).
        - [x] **Mesh Collider** : Pouvoir utiliser la géométrie des `bb3d::Mesh` comme collision statique.

## ⚡ Optimisations (Priorité Haute)
- [x] 🕵️ **Frustum Culling (CPU side)** : Ne pas envoyer au GPU les objets hors du champ de vision de la caméra (utilisation des AABB).
- [ ] 🚀 **Async Texture Upload** : Remplacer `beginSingleTimeCommands` (bloquant) par une file de transfert dédiée et des Fences pour le chargement non-bloquant.
- [ ] 📦 **Material Storage Buffer** : Remplacer les UBOs par matériau par un unique SSBO global (Material Array) pour réduire le binding et l'overhead mémoire.
- [ ] 💡 **Dynamic Lights (SSBO)** : Supprimer la limite de 10 lumières en passant les données d'éclairage dans un SSBO redimensionnable.
- [ ] 🔗 **Bindless Textures (Descriptor Indexing)** : Utiliser un tableau global de textures pour éliminer les changements de Descriptor Sets.
- [ ] 🛡️ **Z-Prepass** : Passe de profondeur initiale pour réduire l'overdraw et économiser le fragment shader PBR.
- [x] 🏎️ **Optimisation du JobSystem** : Affiner la répartition pour le culling et les mises à jour de transforms.
- [x] 🗺️ **Mipmapping & Compression (BC7)** : Réduire la bande passante mémoire et améliorer la qualité visuelle au loin.
- [ ] 📉 **LOD (Level of Detail)** : Système de switch de modèles basé sur la distance pour réduire le nombre de triangles.
- [ ] 💾 **Pipeline Cache** : Sauvegarder l'état des pipelines sur disque pour un démarrage instantané.
- [ ] ⚡ **GPU-Driven Rendering** : Utiliser `DrawIndirect` pour laisser le GPU gérer totalement la liste d'affichage.

## 🚀 Features (Gameplay & Rendu)
- [ ] 🔊 **Système Audio (miniaudio)** : Support des sons 3D spatialisés et gestion sources/listeners.
- [ ] 🖼️ **Render To Texture (RTT)** : Base du post-processing.
    - [ ] **Classe RenderTarget** : Wrapper Vulkan pour images attachables (Color + Depth).
    - [ ] **Fullscreen Quad** : Shaders et Pipeline pour afficher une texture plein écran.
    - [ ] **Refactor Renderer** : Pipeline en 2 passes (Scene -> Texture -> Swapchain).
- [ ] 🌑 **Shadow Mapping** : Implémenter les ombres portées (Cascaded Shadow Maps).
- [ ] 🪞 **Image Based Lighting (IBL)** : Utiliser la Skybox pour des reflets et un éclairage ambiant réaliste.
- [ ] 🏔️ **Terrain System** : Rendu de grands terrains via Heightmaps et LOD.
- [ ] 💨 **Particle System** : Système de particules GPU (Compute shaders).
- [ ] 🎬 **Post-Processing** : Bloom, SSAO, Motion Blur.

## 🛠️ Outils & DX (Developer Experience)
- [ ] 🖥️ **Intégration ImGui** : Interface de debug pour manipuler la scène en temps réel.
- [ ] 🔄 **Hot Shader Reloader** : Recompilation automatique des shaders à la volée.
- [ ] 🎮 **Scene Editor** : Gizmos de manipulation (Translation/Rotation/Scale) dans la vue 3D.
- [ ] 📚 **Doxygen** : Documentation technique complète.

## 🧪 Tests & Qualité
- [ ] 📈 **Stress Test Instancing** : Tester la limite avec 10 000+ objets animés.
- [ ] 🧼 **Nettoyage Validation Layers** : Corriger les derniers warnings de layout/interface.
- [x] 🧱 **Tests Physiques** :
    - [x] **unit_test_18_physics_basic** : Une boîte tombe sur un plan statique.
    - [x] **unit_test_19_physics_stacks** : Une pyramide de boîtes pour tester la stabilité et tir de projectiles.
    - [x] **Demo Integration** : Intégrer la physique dans la démo principale (avions qui tombent ?).

## ⚙️ Refactoring
- [ ] ⚡ **Renderer Allocations** : 
    - [ ] Éviter la création de `std::string` dans `getMaterialForTexture` (Hot Path).
    - [ ] Réutiliser le vecteur `RenderCommand` (reserve/clear) au lieu de réallouer à chaque frame.
- [ ] ♻️ **Mesh Update** : Optimiser `Mesh::updateVertices` pour éviter la re-création complète des buffers (utiliser Staging ou Mapping persistant).
- [ ] 🧩 **Modularisation du Renderer** : Découpler la Swapchain et les Pipelines du Renderer global.
- [ ] 📐 **Standardisation Vertex Layout** : Vérification stricte du SSOT (Single Source of Truth).
