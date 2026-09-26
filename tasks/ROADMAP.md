# 🗺️ Feuille de Route Stratégique & Backlog - biobazard3d (`bb3d`)

Ce document constitue le **plan de route officiel** du moteur `bb3d`. Il organise les chantiers techniques et fonctionnels en **4 jalons séquentiels et cohérents**, garantissant la stabilité, la modernité de l'API graphique Vulkan et les capacités de rendu temps réel.

---

## 🧭 Fonctionnement & Prise en Charge de Tâches

Le développement de `bb3d` est conçu pour permettre le travail collaboratif sans interférence entre développeurs et agents IA (Antigravity, OpenCode, Mistral Vibe Coda, etc.) :

1. **Sélection :** Choisir un chantier prioritaire ci-dessous ou un bug du catalogue [`CODE_REVIEW.md`](CODE_REVIEW.md).
2. **Réservation & Fiche :**
   - Créer ou mettre à jour une fiche dans [`active/`](active/) (ex: `tasks/active/TASK-XXX.md`) basée sur [`templates/TASK_REVIEW_TEMPLATE.md`](templates/TASK_REVIEW_TEMPLATE.md).
   - Indiquer le statut `[READY FOR ARCHITECTURE REVIEW]`, l'Auteur (`@dev`) et le(s) Reviewer(s).
3. **Branche Git :** Isoler le développement sur une branche dédiée (ex: `feat/nom-de-tache` ou `fix/b8-material-ubo`).
4. **Approbation d'Architecture :** Valider formellement la conception avant tout codage source.
5. **Cycle TDD :** Rédiger le test unitaire en premier (`tests/unit_test_*.cpp`), implémenter la solution minimale, et valider la suite de tests :
   ```bash
   cmake --build build --config Debug -j && ctest --test-dir build -C Debug --output-on-failure
   ```
6. **Revue Croisée :** Valider les checkpoints techniques Implémenteur & Reviewer dans la fiche `TASK-XXX.md`.
7. **Clôture & Historique :**
   - Consigner **1 entrée compacte (3 lignes max)** dans [`HISTORY.md`](HISTORY.md).
   - Déplacer la fiche vers [`archive/`](archive/).

---

## 🎯 Jalons Stratégiques (Milestones)

```mermaid
flowchart LR
    J1["Jalon 1<br><b>Fiabilisation & Bugs</b><br><i>Stabilité & Sécurité</i>"] --> J2["Jalon 2<br><b>Socle Vulkan Moderne</b><br><i>Sync2 & Standards</i>"]
    J2 --> J3["Jalon 3<br><b>Rendu Avancé & Gameplay</b><br><i>CSM, Audio, Multi-Streams</i>"]
    J3 --> J4["Jalon 4<br><b>Outils & Éditeur ImGui</b><br><i>Viewport & Inspection</i>"]
```

---

### 🛡️ Jalon 1 : Fiabilisation & Résolution des Bugs Critiques
> **Objectif :** Éliminer les conditions de course GPU, deadlocks, comportements indéfinis et fuites de mémoire identifiés dans [`tasks/CODE_REVIEW.md`](CODE_REVIEW.md) avant d'ajouter de nouvelles couches architecturales.

- [x] **Course CPU/GPU UBO Matériaux (`B8`)** : Synchronisation activée via `Material::SetCurrentFrame(m_currentFrame)` au début de `Renderer::render()`, test unitaire validé (`unit_test_25_material_frame`).
- [x] **Sécurité Swapchain Resize (`B1`)** : Reconstruction propre et symétrique de `m_renderFinishedSemaphores` selon le nouveau `imageCount` lors du resize.
- [x] **Deadlock de Fence sur échec Submit (`B2`)** : Re-signalement propre de `m_inFlightFences` dans le catch de `submitAndPresent()` pour éliminer tout deadlock à la frame suivante.
- [x] **Offset Batch Ombres (`B3`)** : Réinitialisation `lastMesh = nullptr` sur le saut de non-caster dans `renderShadows()` garantissant des matrices d'ombres correctes.
- [x] **Picking GPU & Fuites de Sets (`B4`, `B5`, `B6`)** :
  - Libération propre des descriptor sets de picking via `freeDescriptorSets` et préservation des buffers lors des redimensionnements (`B4`).
  - Allocation eager des pipelines/ressources à l'init et redimensionnement d'images dédié dans `m_resizeRequested` hors `cb.begin()`, éliminant les `dev.waitIdle()` mid-frame (`B5`).
  - Synchronisation de readback par fence isolée (`m_pickingFence`) et pool transitoire dédié, éliminant le stall `queue.waitIdle()` global (`B6`), test unitaire validé (`unit_test_26_picking`).
- [x] **Robustesse Physique Jolt (`B14`, `B15`, `B16`, `B20`, `B21`)** :
  - Clamper le nombre de threads (`std::max(1, hardware_concurrency - 1)`) (`B14`).
  - Ajouter des gardes `entity.has<TransformComponent>()` dans `createRigidBody` et `createCharacterController` (`B15`, `B20`).
  - Sécuriser l'allocation de corps (`CreateBody != nullptr`) (`B16`).
- [x] **JobSystem Busy-Poll (`B24`, `B25`)** : Architecture hybride Spin-Then-Park (`_mm_pause` hot path + park OS au repos), 0% CPU idle, wake latency 30 µs, test unitaire validé (`unit_test_08_core_systems`).
- [x] **Nettoyage du Code Mort & Fichiers Obsolètes (`D3`, `D4`, `D5`, `ND1`)** :
  - Supprimer `m_instanceTransforms` non lu (`D3`).
  - Supprimer `getMaterialForTexture` et `m_defaultMaterials` inutilisés (`D4`).
  - Supprimer le bloc d'horizon culling commenté (`D5`).
  - Supprimer `StagingBuffer::submitCopy` non appelé et buggé (`ND1`).

---

### ⚡ Jalon 2 : Socle Vulkan 1.3 / 1.4 Moderne & Synchronisation
> **Objectif :** Aligner l'infrastructure graphique sur les standards modernes documentés dans le [Rapport d'Audit Vulkan](../docs/vulkan_audit/RAPPORT_AUDIT_VULKAN_MODERNE.md) (SDK `1.4.335.0` actif).

- [x] **Initialisation Vulkan 1.3/1.4 via `vk::StructureChain`** :
  - Négociation dynamique `VK_API_VERSION_1_4` / fallback 1.3, interrogation préalable type-safe via `m_physicalDevice.getFeatures2(&queryChain)`, activation `StructureChain` des features core (`synchronization2`, `dynamicRendering`, `timelineSemaphore`, `pushDescriptor`, bindless), VMA aligné (`unit_test_02_vulkan_init` PASS).
- [x] **Migration Synchronization2 (`pipelineBarrier2`)** :
  - 18 barrières Vulkan 1.0 éliminées de `Renderer.cpp` et `Texture.cpp`.
  - Utilisation systématique de `vk::DependencyInfo` et `vk::ImageMemoryBarrier2` avec les stages et accès 64-bit (`vk::PipelineStageFlagBits2`, `vk::AccessFlagBits2`).
  - Batching couleur+profondeur via `std::array<vk::ImageMemoryBarrier2, 2>`, 0 pseudo-stage (`eTopOfPipe`/`eBottomOfPipe`), test TDD `unit_test_32_synchronization2` validé (0.74s).
- [x] **Timeline Semaphores & Transferts Réellement Asynchrones (`B11`, `P11`)** :
  - Timeline Semaphore Core 1.3/1.4 dédié à la file de transfert (`m_transferQueue`), 0 appel bloquant `waitForFences(UINT64_MAX)` dans `endTransferCommandsAsync` (B11 résolu).
  - Élimination des fences brutes dans `Texture` (`uint64_t m_uploadTimelineValue`), `Texture::isReady()` 100% non-bloquant.
  - Recyclage différé et proactif des command buffers de transfert dans `Renderer::render()`, test TDD `unit_test_33_timeline_transfers` validé (0.71s).
- [x] **Instrumentation DebugUtils & Profiling Tracy GPU** :
  - Baliser les command buffers et passes de rendu avec `vkCmdBeginDebugUtilsLabelEXT` / `vkCmdEndDebugUtilsLabelEXT` via RAII `ScopedDebugLabel`.
  - Intégrer les zones de timing GPU via `TracyVkZone` (`BB_GPU_ZONE`), collecte par frame `TracyVkCollect`, et nommage type-safe des objets Vulkan (`setObjectName`). Test unitaire `unit_test_34_debug_utils_profiling` validé.
- [x] **Pipeline Cache Persistant** :
  - Sérialiser l'objet `vk::PipelineCache` dans `assets/cache/pipelines.bin` pour éliminer les micro-saccades lors des lancements ultérieurs. Validation du header 32B (UUID/Vendor/Device), sauvegarde atomique, merge dynamique, test TDD 35 PASS (1.20s).
- [x] **Extended Dynamic State (Vulkan 1.3 Core)** :
  - Exploiter les états dynamiques étendus (`eCullMode`, `eFrontFace`, `eDepthTestEnable`, `eDepthWriteEnable`, `eDepthCompareOp`, `ePrimitiveTopology`) pour diviser le nombre de pipelines graphiques requis. Chaînage StructureChain 1.3/1.4, test TDD 37 PASS (3.79s).

---

### 🎨 Jalon 3 : Rendu Graphique Avancé & Gameplay
> **Objectif :** Améliorer la fidélité visuelle, la flexibilité des assets et les systèmes de jeu interactifs.

- [ ] **Améliorations Cascaded Shadow Maps (CSM)** *(Fiche active : [`TASK-CSM_Improvement_Plan.md`](active/TASK-CSM_Improvement_Plan.md))* :
  - Implémenter la répartition pratique des splits (Practical Split Scheme).
  - Normal Bias adaptatif et Texel Snapping stabilisé pour éliminer l'acné d'ombre et le chatoiement.
  - Frustum culling par cascade pour réduire les draw calls redondants.
- [ ] **Découplage Multi-Streams Sommets (Fin de l'Uber-Vertex)** :
  - Séparer la géométrie en flux : `VertexPos` (12 octets) pour les shadow passes, z-prepass et picking, et `VertexStatic` (36 octets) pour les attributs PBR.
  - Réduire l'empreinte bande-passante mémoire de plus de 60% sur les passes de profondeur.
- [ ] **Push Descriptors & Allocateur de Descriptors Moderne** :
  - Remplacer les pools rigides par un allocateur dynamique à blocs (pages de descripteurs).
  - Évaluer les Push Descriptors pour les UBOs et paramètres fréquents.
- [ ] **SSBO Matériaux & Bindless Textures** :
  - Stocker les propriétés des matériaux dans un unique SSBO global indexé par instance.
  - Tableau de textures non dimensionné (Descriptor Indexing / Bindless) pour éliminer les réallocations de bindings de texture par draw call.
- [ ] **Dynamic Lights (SSBO)** :
  - Remplacer le tableau fixe de 10 lumières par un SSBO redimensionnable pour un éclairage riche.
- [ ] **Z-Prepass (Depth Pre-pass)** :
  - Passe de pré-remplissage du depth buffer avec `VertexPos` pour éliminer l'overdraw sur scènes denses.
- [ ] **Système Audio 3D (`miniaudio`)** :
  - Intégration de miniaudio pour les sources sonores 3D spatialisées et le listener.
- [ ] **Post-Processing & Render To Texture (RTT)** :
  - Classe générique `RenderTarget` offscreen.
  - Pipeline de post-traitement composable (Tone Mapping, Bloom, SSAO).

---

### 🎛️ Jalon 4 : Outils, Éditeur ImGui & Ergonomie
> **Objectif :** Offrir un environnement d'édition temps réel modulaire, découplé et interactif (`BB3D_ENABLE_EDITOR`), conforme à la [Spécification Architecturale](../docs/editor/EDITOR_ARCHITECTURE_SPECIFICATION.md).

#### 🧱 Phase 4.1 : Socle Modulaire & Découpage de l'Éditeur
- [x] **Infrastructure Centrale (`EditorContext`, `EditorPanel`)** :
  - Définir l'interface `EditorPanel` (`onImGuiRender`, `onUpdate`, `onEvent`, `getId`, `getTitle`, `isOpen`).
  - Implémenter `EditorContext` comme bus central (scène active, multi-sélection, barycentre pivot, état simulation).
  - Test unitaire TDD : `unit_test_38_editor_context` (validation sélection, multi-sélection, pivot).
- [x] **Orchestration (`EditorPanelManager`) & Refactorisation du Monolithe `ImGuiLayer`** :
  - Alléger `ImGuiLayer` servant uniquement d'hôte de backend ImGui SDL3/Vulkan et délégant à `EditorPanelManager`.
  - Découper `ImGuiLayer.cpp` (allégé de 1161 à 314 lignes) en 6 sous-panneaux modulaires dans `src/bb3d/editor/panels/`.
- [x] **Éradication des Bugs Statiques Répertoriés (N5, N6, N7)** :
  - Supprimer la branche `else if` dupliquée vide rétablissant l'inspecteur `LightComponent` (`N5`).
  - Supprimer le statique partagé `partCol` pour une couleur par entité/composant (`N6`).
  - Supprimer le statique partagé `s_loadConfig` pour un preset par asset/entité (`N7`).

#### ⏪ Phase 4.2 : Système de Commande & Historique Undo/Redo
- [ ] **Moteur de Commandes Borné (`EditorCommand`, `CommandHistory`)** :
  - Pile circulaire ou deque bornée à 100 actions avec support de fusion (merging) des manipulations continues.
  - Test unitaire TDD : `unit_test_37_editor_command_history` (validation Undo, Redo, limite mémoire, dirty flag).
- [ ] **Commandes ECS Réversibles** :
  - `TransformEntityCommand` (mémorisation de translation, rotation, scale avant/après manipulation).
  - `CreateEntityCommand` & `DestroyEntityCommand` (instanciation et sérialisation/restauration JSON complète).
  - `ChangePropertyCommand<T>` pour les modifications d'attributs de composants.
- [ ] **Raccourcis & Menu Édition** :
  - Raccourcis universels `Ctrl+Z` (Annuler), `Ctrl+Y` / `Ctrl+Shift+Z` (Rétablir) intégrés au menu principal.

#### 🕹️ Phase 4.3 : Manipulation 3D Temps Réel & Gizmos ImGuizmo
- [ ] **Intégration CMake d'ImGuizmo** :
  - Déclaration `FetchContent_Declare(imguizmo ...)` sous `#if defined(BB3D_ENABLE_EDITOR)`.
- [ ] **Intégration Mathématique dans le Viewport 3D** :
  - Adaptation des matrices View et Projection pour Vulkan (inversion Y) sans casser le rendu.
  - Support complet des modes Translation (`W`), Rotation (`E`), Échelle (`R`) en repère World vs Local.
  - Snapping paramétrable (grille 0.5m, angle 15°, scale 0.1x).
- [ ] **Multi-Sélection & Pivot Barycentrique** :
  - Calcul du centre de masse de la sélection multiple et transformation simultanée du groupe.
  - Synchronisation temps réel des transforms avec Jolt Physics (`updateBodyTransform`).
  - Génération d'une commande unique d'Undo lors du relâchement du gizmo (`!ImGuizmo::IsUsing()`).

#### 📂 Phase 4.4 : Ergonomie de Rendu, Navigation & Content Browser
- [ ] **Caméra Éditeur Dédiée (`EditorCamera`)** :
  - Caméra libre Flycam (WASD + Clic Droit, vitesse réglable à la molette) et mode Orbit (Alt + Clic Gauche).
  - Cadrage automatique instantané de l'entité sélectionnée avec la touche `F`.
  - Bascule en un clic entre Caméra Éditeur libre et Caméra Jeu active.
- [ ] **Asset Browser Interactif (`AssetBrowserPanel`)** :
  - Arborescence de dossiers sous `assets/` et affichage en grille d'icônes avec cache de miniatures.
- [ ] **Système Universel de Drag & Drop (Natif ImGui)** :
  - Déposer un maillage 3D (`.obj`, `.gltf`) dans le Viewport instancie le modèle au sol.
  - Déposer une texture (`.png`, `.jpg`) sur un slot de l'Inspecteur l'assigne au matériau PBR.
  - Déposer un fichier `.json` charge la scène correspondante.
- [ ] **Console de Logs Temps Réel (`ConsolePanel`)** :
  - Sink mémoire circulaire `EditorConsoleSink` pour `spdlog` avec filtres par niveau (Trace/Info/Warn/Error).
  - Recherche textuelle, auto-scroll et copie presse-papier.

#### 📊 Phase 4.5 : Diagnostics Temps Réel & Paramétrage Avancé
- [ ] **Moniteur de Métriques & StatsOverlay** :
  - Overlay HUD semi-transparent dans le Viewport (FPS, frametime CPU/GPU, draw calls, triangles, VRAM VMA).
  - Décomposition des passes Tracy GPU (`BB_GPU_ZONE`).
- [ ] **Configuration de Scène & Environnement (`SceneSettingsPanel`)** :
  - Paramétrage interactif des biais de Cascaded Shadow Maps (CSM), brouillard atmosphérique, skybox.
  - Réglages du post-process (Tone Mapping, Bloom).
- [ ] **Persistance des Dispositions de Fenêtres (Docking Layouts)** :
  - Sauvegarde et chargement de `editor_layout.ini`.
  - Presets de disposition : "Défaut", "Level Design", "Animation", "Debug & Profilage".


---

## 📦 Archive des Réalisations (Fondations Validées)

Toutes les briques fondamentales ci-dessous ont été validées et archivées :

- [x] 🏗️ **Core Architecture** : Singleton Engine, Windowing SDL3, Logging spdlog, Profiling Tracy.
- [x] 🎨 **Vulkan Backend Init** : Initialisation Vulkan 1.3, Dynamic Rendering natif sans RenderPass, allocation mémoire VMA.
- [x] 💎 **Descriptor Management** : Triple Buffering pour les Descriptor Sets des matériaux.
- [x] 📦 **Asset Loading** : Chargeur OBJ (`tinyobjloader`) et glTF 2.0 (`fastgltf`) avec extraction des textures et matériaux.
- [x] 💎 **PBR Rendering** : Modèle Cook-Torrance complet (Albedo, Normal, ORM, Emissive).
- [x] ⚡ **GPU Instancing** : Batching automatique par mesh/matériau via SSBO d'instances.
- [x] 💡 **Multi-Lights Base** : Support initial de 10 lumières simultanées (directionnelles et ponctuelles).
- [x] ✨ **Cel-Shading** : Rendu stylisé toon avec quantification et outlines.
- [x] 📂 **Serialization 2.0** : Sauvegarde et rechargement de scènes complètes au format JSON.
- [x] 🧵 **JobSystem & ECS** : Thread pool multithread et registre de composants EnTT optimisé.
- [x] 📐 **Maths & Camera** : Intégration GLM, caméras FPS et Orbitale interactives.
- [x] 🌍 **Intégration Jolt Physics** : RigidBodies dynamiques/statiques, colliders, raycasting et character controller de base.
- [x] 🕵️ **Frustum Culling CPU** : Rejet des objets hors champ via les AABB.
- [x] 🗺️ **Compression Textures** : Support du format compressé BC7 et génération de mipmaps.
