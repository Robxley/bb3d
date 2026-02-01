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
- [x] 🌍 **Intégration Jolt Physics** : Simulation réelle (RigidBodies, Colliders, Raycasting, Character Controller).

## ⚡ Optimisations (Priorité Haute)
- [x] 🕵️ **Frustum Culling (CPU side)** : Ne pas envoyer au GPU les objets hors du champ de vision de la caméra (utilisation des AABB).
- [ ] 🔗 **Bindless Textures (Descriptor Indexing)** : Utiliser un tableau global de textures pour éliminer les changements de Descriptor Sets.
- [ ] 🛡️ **Z-Prepass** : Passe de profondeur initiale pour réduire l'overdraw et économiser le fragment shader PBR.
- [x] 🏎️ **Optimisation du JobSystem** : Affiner la répartition pour le culling et les mises à jour de transforms.
- [x] 🗺️ **Mipmapping & Compression (BC7)** : Réduire la bande passante mémoire et améliorer la qualité visuelle au loin.
- [ ] 📉 **LOD (Level of Detail)** : Système de switch de modèles basé sur la distance pour réduire le nombre de triangles.
- [ ] 💾 **Pipeline Cache** : Sauvegarder l'état des pipelines sur disque pour un démarrage instantané.
- [ ] ⚡ **GPU-Driven Rendering** : Utiliser `DrawIndirect` pour laisser le GPU gérer totalement la liste d'affichage.

## 🚀 Features (Gameplay & Rendu)
- [ ] 🔊 **Système Audio (miniaudio)** : Support des sons 3D spatialisés et gestion sources/listeners.
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
- [x] 🧱 **Tests Physiques** : Validation de la chute et des collisions (unit_test_18).

## ⚙️ Refactoring
- [ ] 🧩 **Modularisation du Renderer** : Découpler la Swapchain et les Pipelines du Renderer global.
- [ ] 📐 **Standardisation Vertex Layout** : Vérification stricte du SSOT (Single Source of Truth).
