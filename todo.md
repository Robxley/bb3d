# 📝 biobazard3d - TODO List

Ce document suit l'évolution du moteur biobazard3d. Les tâches terminées sont marquées pour garder une trace de la progression.

## ✅ Terminé (Archive des Features)
- [x] 🏗️ **Core Architecture** : Singleton Engine, Windowing (SDL3), Logging (spdlog), Profiling (Tracy).
- [x] 🎨 **Vulkan Backend** : Initialisation Vulkan 1.3, Dynamic Rendering (sans RenderPass legacy), VMA.
- [x] 💎 **Descriptor Management** : Implémentation du **Triple Buffering** pour les Descriptor Sets des matériaux (élimination des freezes et erreurs de synchronisation).
- [x] 📦 **Asset Loading** : Chargeur OBJ (tinyobjloader) et glTF 2.0 (fastgltf) avec support des matériaux.
- [x] 💎 **PBR Rendering** : Modèle Cook-Torrance complet (Albedo, Normal, ORM, Emissive).
- [x] ⚡ **GPU Instancing** : Batching automatique via SSBO pour dessiner des milliers d'objets en un seul Draw Call.
- [x] 💡 **Multi-Lights** : Support de 10 lumières simultanées (Directional & Point) avec atténuation physique.
- [x] ✨ **Cel-Shading** : Rendu cartoon avec quantification des couleurs et Outlines (contours).
- [x] 📂 **Serialization 2.0** : Système de sauvegarde/chargement JSON amélioré avec reconstruction des primitives (Cube, Sphère, Plan) et persistance des couleurs.
- [x] 🧵 **JobSystem** : Thread Pool multi-coeur pour les tâches asynchrones.
- [x] 📐 **Maths & Camera** : Intégration GLM, Caméras FPS et Orbitale interactives.
- [x] 🧹 **Test System Harmonization** : Collecte automatique des tests, renommage cohérent, et correction du déploiement des assets (zéro conflit).
- [x] 🪟 **Window Resizing** : Gestion robuste du redimensionnement et de la minimisation (Swapchain recreation).
- [x] 🧩 **ECS & View Architecture** : Refonte vers un ECS pur (Composants de contrôle séparés) et introduction de `View<T>` pour un accès typé sans overhead.
- [x] 🌍 **Intégration Jolt Physics** : Simulation réelle avec RigidBodies, Colliders, Raycasting et Character Controller.
- [x] 🧹 **Physics Cleanup** : Correction des "objets fantômes" via un `PhysicsWorld::clear()` lors du rechargement de scène.

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

## 🛠️ Outils & Editeur (ImGui)
*Module optionnel, activé uniquement en mode `BB3D_ENABLE_EDITOR`.*

- [ ] 📥 **Intégration Dépendance**
    - [ ] Ajouter `dear imgui` via CMake (FetchContent ou Submodule).
    - [ ] Activer les backends `imgui_impl_sdl3` et `imgui_impl_vulkan`.
    - [ ] Configurer la macro `BB3D_ENABLE_EDITOR` pour l'exclusion du code en Release.

- [ ] 🏗️ **Core Layer (Abstraction)**
    - [ ] Créer la classe `bb3d::ImGuiLayer`.
    - [ ] `Init()` : Initialiser le contexte ImGui, le Style (Dark Theme), et activer le **Docking** (`ImGuiConfigFlags_DockingEnable`).
    - [ ] `InitVulkan()` : Créer le DescriptorPool dédié (requis par ImGui pour les polices et textures).
    - [ ] `BeginFrame()` : Wrapper `ImGui_ImplVulkan_NewFrame` et `ImGui_ImplSDL3_NewFrame`.
    - [ ] `EndFrame()` : Appel à `ImGui::Render()` et enregistrement des DrawCmds dans le CommandBuffer fourni.
    - [ ] `OnEvent()` : Intercepter les événements SDL3. Si `io.WantCaptureMouse` est true, bloquer la propagation vers le moteur.

- [ ] 🖼️ **Viewport Rendering (Scene-in-UI)**
    - [ ] **Texture Descriptor** : Créer un `VkDescriptorSet` via `ImGui_ImplVulkan_AddTexture` pour la texture de sortie du `RenderTarget`.
    - [ ] **Viewport Window** : Créer une fenêtre ImGui "Scene" qui affiche cette texture via `ImGui::Image`.
    - [ ] **Aspect Ratio Handling** : Ajuster la caméra du jeu en fonction de la taille de la fenêtre ImGui (et non plus de la fenêtre OS).
    - [ ] **Input Mapping** : Convertir les coordonnées souris "écran" en coordonnées "viewport" pour le picking d'objets.

- [ ] 🔌 **Intégration Moteur**
    - [ ] Modifier `Engine` pour posséder un `Scope<ImGuiLayer>` (optionnel).
    - [ ] Modifier `Renderer` pour accepter un callback de rendu d'overlay ou appeler `ImGuiLayer::Render` à la fin de la passe principale.
    - [ ] Ajouter un flag `enableEditor` dans `engine_config.json`.

- [ ] 🎛️ **Panneaux & Fonctionnalités (Editor)**
    - [ ] **Scene Hierarchy** : Lister les entités, sélection, parentage.
    - [ ] **Inspector** : Modifier les composants de l'entité sélectionnée (Transform, Light, Material).
    - [ ] **Stats Panel** : Afficher FPS, Temps CPU/GPU, Nombre de DrawCalls, RAM VMA utilisée.
    - [ ] **Log Console** : Sink spdlog personnalisé pour afficher les logs dans une fenêtre ImGui.
    - [ ] **Gizmos** : (Future) Intégrer `ImGuizmo` pour manipuler les objets dans la vue 3D.

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