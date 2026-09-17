# **🗺️ Roadmap & To-Do de Développement \- biobazard3d**

Ce document centralise le plan de développement du moteur **biobazard3d**, ainsi que toutes les tâches (Features, Optimisations, Refactoring) en cours ou à venir.

**Philosophie :** Chaque étape (Milestone) est validée par un exécutable de test unitaire autonome (Sandbox) qui prouve le fonctionnement du module de manière isolée avant l'intégration globale. De plus, chaque nouvelle fonctionnalité passe obligatoirement par un processus de Brainstorming et de Planification documenté dans `tasks/active/`.

---

## **✅ Terminé (Archive des Features Majeures)**

- [x] 🏗️ **Core Architecture** : Singleton Engine, Windowing (SDL3), Logging (spdlog), Profiling (Tracy).
- [x] 🎨 **Vulkan Backend** : Initialisation Vulkan 1.3, Dynamic Rendering (sans RenderPass legacy), VMA.
- [x] 💎 **Descriptor Management** : Implémentation du **Triple Buffering** pour les Descriptor Sets des matériaux.
- [x] 📦 **Asset Loading** : Chargeur OBJ (tinyobjloader) et glTF 2.0 (fastgltf) avec support des matériaux.
- [x] 💎 **PBR Rendering** : Modèle Cook-Torrance complet (Albedo, Normal, ORM, Emissive).
- [x] ⚡ **GPU Instancing** : Batching automatique via SSBO.
- [x] 💡 **Multi-Lights** : Support de 10 lumières simultanées (Directional & Point) avec atténuation.
- [x] ✨ **Cel-Shading** : Rendu cartoon avec quantification des couleurs et Outlines.
- [x] 📂 **Serialization 2.0** : Système de sauvegarde/chargement JSON amélioré avec reconstruction des primitives.
- [x] 🧵 **JobSystem & ECS** : Thread Pool multi-coeur et refonte vers un ECS pur avec `View<T>`.
- [x] 📐 **Maths & Camera** : Intégration GLM, Caméras FPS et Orbitale interactives.
- [x] 🌍 **Intégration Jolt Physics** : Simulation temps réel avec RigidBodies, Colliders, Raycasting et Character Controller.

---

## **📅 Phase Actuelle : Gameplay, Outils & Rendu Avancé**

### **🛠️ Outils & Editeur (ImGui)**
*Module optionnel, activé uniquement en mode `BB3D_ENABLE_EDITOR`.*

- [ ] 📥 **Intégration Dépendance** : Ajouter `dear imgui`, activer les backends SDL3/Vulkan, configurer macro.
- [ ] 🏗️ **Core Layer (Abstraction)** : Créer `bb3d::ImGuiLayer` (Init, BeginFrame, EndFrame, Event Intercept).
- [ ] 🖼️ **Viewport Rendering** : Fenêtre "Scene" ImGui, Texture Descriptor, Aspect Ratio et Input Mapping.
- [ ] 🔌 **Intégration Moteur** : Hooks dans `Engine` et `Renderer` (`enableEditor` via JSON).
- [ ] 🎛️ **Panneaux & Fonctionnalités** : Hierarchie, Inspector, Stats Panel, Log Console, et Gizmos.

### **🚀 Features (Gameplay & Rendu)**
- [ ] 🔊 **Système Audio (miniaudio)** : Support des sons 3D spatialisés et gestion sources/listeners.
- [ ] 🖼️ **Render To Texture (RTT)** : Classe RenderTarget et Fullscreen Quad pour pipeline post-process.
- [ ] 🌑 **Shadow Mapping** : Implémenter les ombres portées (Cascaded Shadow Maps).
- [ ] 🪞 **Image Based Lighting (IBL)** : Skybox pour reflets et éclairage ambiant réaliste.
- [ ] 🏔️ **Terrain System** : Rendu de grands terrains via Heightmaps.
- [ ] 💨 **Particle System** : Système de particules GPU (Compute shaders).
- [ ] 🎬 **Post-Processing** : Bloom, SSAO, Motion Blur.

---

## **⚡ Optimisations & Refactoring Techniques**

### **🟢 Priorité Haute (Gain immédiat)**
- [x] 🕵️ **Frustum Culling (CPU side)** : Utilisation des AABB.
- [x] 🏎️ **Optimisation du JobSystem** : Parallélisation du Culling & Tri terminé.
- [x] 🗺️ **Mipmapping & Compression (BC7)** : Réduction de la BP globale.
- [x] 🧹 **Élimination Goulots Allocations (Heap Pressure)** : Terminée (reserve/clear).
- [ ] 🚀 **Vulkan Synchronization2 (`pipelineBarrier2`)** : Modernisation complète des barrières mémoire vers l'API Vulkan 1.3+ (*cf. [Audit Vulkan](vulkan_audit/RAPPORT_AUDIT_VULKAN_MODERNE.md)*).
- [ ] ⏱️ **Timeline Semaphores & Async Queue** : Remplacement des semaphores binaires et `waitIdle()` bloquants par des Timeline Semaphores et file de transfert dédiée.
- [ ] 🎛️ **Push Descriptors & Descriptor Allocator** : Élimination de l'allocation de descriptor sets par matériau et correction des fuites de pool.
- [ ] 📦 **Material Storage Buffer** : Remplacer les UBOs / matériaux par un unique SSBO global (Material Array).
- [ ] 💡 **Dynamic Lights (SSBO)** : Supprimer la limite des 10 lumières via un SSBO redimensionnable.
- [ ] 🔗 **Bindless Textures (Descriptor Indexing)** : Tableau global pour éliminer les changements de bindings.
- [ ] 🛡️ **Z-Prepass** : Passe de profondeur initiale (Depth Pre-pass).

### **🟡 Priorité Moyenne (Refactoring & CPU)**
- [ ] 📐 **Découplage Vertex Layout (Multi-Streams)** : Séparer `VertexPos` (12o) pour shadows/Z-prepass de `VertexStatic` (36o), fin de l'Uber-Vertex unique (conforme GEMINI.md).
- [ ] 🎚️ **Extended Dynamic State (Vulkan 1.3)** : Cull mode, depth compare et primitive topology dynamiques pour réduire la multiplication des pipelines.
- [ ] 💾 **Pipeline Cache Persistant** : Sauvegarder le blob binaire `vk::PipelineCache` sur disque (`assets/cache/pipelines.bin`) pour éliminer les micro-saccades au démarrage.
- [ ] 🏷️ **Vulkan DebugUtils Labels & Tracy GPU** : Baliser chaque passe de rendu avec `vkCmdBeginDebugUtilsLabelEXT` et instrumenter avec `TracyVkZone`.
- [ ] ♻️ **Mesh Update** : Optimiser `Mesh::updateVertices` avec un Mapping persistant ou Staging.
- [ ] 🧩 **Modularisation** : Découpler Swapchain et Pipelines du global `Renderer`.
- [ ] 📉 **LOD (Level of Detail)** : Switch de modèles ou tesselation basée sur la distance.
- [ ] 🛡️ **Libération RAM Mesh** : Appeler `releaseCPUData()` auto sur les meshes statiques.

### **🔴 Priorité Basse / Recherche**
- [ ] ⚡ **GPU-Driven Rendering** : `DrawIndirect` + Compute Shader Culling.
- [ ] 🧪 **Stress Test Instancing** : Faire une démo de benchmark à > 10 000 objets animés.
- [ ] 🧼 **Nettoyage Validation Layers** : Activer `Synchronization Validation` en debug et corriger les derniers avertissements SPIR-V.
- [ ] 🧠 **Initialisation Réactive Physique** : Utiliser les observers EnTT pour générer les RigidBodies Jolt à la volée.
- [ ] 🛡️ **Migration progressive `vk::raii`** : Sécurisation RAII des objets Vulkan internes.

