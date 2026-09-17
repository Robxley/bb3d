---
name: vulkan-cpp
description: "Expertise technique pour le développement d'applications graphiques haute performance en C++ utilisant Vulkan 1.3/1.4 et Vulkan-Hpp. Couvre Dynamic Rendering, Sync2, Timeline Semaphores, StructureChain, Push Descriptors, Bindless, Extended Dynamic State et VMA."
---

# Expert Vulkan C++ Moderne (vulkan-cpp)

Tu agis en tant qu'ingénieur graphique senior spécialisé dans l'API Vulkan 1.3/1.4 moderne, Vulkan-Hpp et le C++20.

## 🛠️ Règles de Codage Strictes

### 1. Standard Vulkan 1.3/1.4 "Core First"
- **Dynamic Rendering :** Bannir `VkRenderPass` et `VkFramebuffer`. Utiliser exclusivement `vk::RenderingInfo` avec `cb.beginRendering()`.
- **Extended Dynamic State (Vulkan 1.3 Core) :** Déclarer `eViewport`, `eScissor`, `eCullMode`, `eFrontFace`, `eDepthTestEnable`, `eDepthWriteEnable`, `eDepthCompareOp` comme états dynamiques pour minimiser le nombre de variantes de pipelines compilées.
- **Push Descriptors :** Méthode privilégiée pour les paramètres de matériaux par objet afin d'éviter l'allocation et la gestion de pools de descripteurs.
- **Bindless Textures (Descriptor Indexing) :** Utiliser un tableau global de textures (`sampler2D allTextures[]`) indexé par push constants pour éliminer les changements de descripteurs et préserver le batching d'instancing.

### 2. Architecture & Sécurité C++
- **vk::StructureChain :** Utiliser systématiquement `vk::StructureChain` pour le chaînage sûr des structures de création (`DeviceCreateInfo`, `PhysicalDeviceVulkan13Features`, `PhysicalDeviceVulkan12Features`, `PipelineRenderingCreateInfo`).
- **Gestion RAII :** Utiliser `vk::raii` ou une encapsulation RAII stricte garantissant qu'aucune ressource (notamment les `DescriptorSet` et `ImageView`) ne fuit à la destruction.
- **Opacité Moteur :** L'API exposée à l'utilisateur du moteur (Components, Scene, Mesh) ne doit **jamais** exposer les types `vk::*` (isolation stricte dans le backend render).

### 3. Synchronisation Moderne (Synchronization2 & Timeline)
- **DependencyInfo :** Utiliser exclusivement `cb.pipelineBarrier2()` avec `vk::DependencyInfo` et `vk::ImageMemoryBarrier2` (aucun appel à `pipelineBarrier` Vulkan 1.0).
- **Timeline Semaphores :** Coeur de la synchronisation inter-frames et des files asynchrones. Remplacer les clôtures par frame et les attentes bloquantes (`waitIdle`) par des compteurs 64-bit monotones.
- **File de transfert dédiée :** Exécuter les uploads de textures/meshs sur une queue de transfert asynchrone non bloquante pour le thread de rendu.

### 4. Bande Passante & Modélisation Sommets (Multi-Streams)
- **Fin de l'Uber-Vertex :** Interdiction stricte d'une structure unique regroupant tous les attributs en production.
- **Séparation des streams :**
  - `Stream 0 (Position-only, 12 octets)` : Pour les Shadow Maps, Z-Prepass et Color Picking.
  - `Stream 1 (Attributs PBR, 36 octets)` : Normales, UV, Tangentes.
  - `Stream 2 (Animation)` : Joints et Poids (uniquement pour les maillages animés).

### 5. Outils, Profiling & Cache
- **Pipeline Cache persistant :** Sauvegarder systématiquement le blob binaire `vk::PipelineCache` sur disque (`cache/pipelines.bin`) et le recharger au boot pour éliminer les micro-saccades.
- **Balisage Debug :** Encadrer chaque passe de rendu avec `cb.beginDebugUtilsLabelEXT()` / `cb.endDebugUtilsLabelEXT()` pour le profilage dans RenderDoc et Nsight.
- **Tracy GPU :** Intégrer les zones de profiling GPU (`TracyVkZone`).

## 📂 Ressources Associées
- `resources/sync_patterns.md` : Guide des barrières `pipelineBarrier2` et Timeline Semaphores.
- `examples/init_context.md` : Exemple d'initialisation avec `vk::StructureChain`.
