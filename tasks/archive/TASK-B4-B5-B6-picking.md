# TASK-B4-B5-B6 : Corrections Robustesse & Performance GPU Color Picking (B4, B5, B6)

- **Statut :** [DONE]
- **Auteur / Implémenteur :** @Antigravity
- **Reviewer(s) :** @bb3d-reviewer (Vibe CLI / GLM-5.2) & @User
- **Branche Git :** `main` (ou `fix/picking-b4-b5-b6`)
- **Date de création :** 2026-09-17

---

## 1. Contexte & Objectif
Le sous-système de Color Picking GPU (`Renderer.cpp` & `PickingSystem.cpp`) permet d'identifier l'entité sous le curseur en effectuant un rendu des IDs d'entités dans une image `R32_UINT`, puis en effectuant un readback 1-pixel vers un buffer staging CPU.
Trois anomalies ont été identifiées dans le catalogue `tasks/CODE_REVIEW.md` :
1. **B4 (Fuite de Descriptor Sets)** : Lors des redimensionnements de fenêtre, `m_pickingDescriptorSets.clear()` vide le vecteur sans libérer les `vk::DescriptorSet` du pool, épuisant progressivement le pool de 2000 sets.
2. **B5 (Stall Pipeline & Allocs Mid-Frame)** : `createPickingResources()` est appelé paresseusement dans `renderEntityIds()` en plein milieu d'un command buffer actif (`cb.begin()` déjà appelé), invoquant `dev.waitIdle()` et créant pipelines et buffers au milieu du rendu.
3. **B6 (Stall Queue Graphique & Contention de Command Pool)** : `readEntityIdAt()` alloue depuis `m_commandPool` (partagé avec la frame), soumet sans fence, puis invoque `m_graphicsQueue.waitIdle()`, figeant l'ensemble du GPU sur le thread principal.

L'objectif est d'éliminer ces trois défauts, de rendre l'initialisation et le redimensionnement prédictifs et hors de la boucle de commande, et d'isoler la lecture de pixel via une synchronisation légère par fence.

---

## 1.bis Revalidation Critique du Bug (Double Check)

### Bug B4 : Fuite de Descriptor Sets sur Redimensionnement
- **Bug ID / Signalement :** B4 dans `tasks/CODE_REVIEW.md` (`Renderer.cpp:1066-1077, 1082-1094`)
- **Diagnostic critique indépendant :** `m_pickingDescriptorSets` stocke des handles alloués depuis `m_descriptorPool`. Le pool est explicitement créé avec le flag `vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet` (`Renderer.cpp:225`). Dans `createPickingResources()` (ligne 1068), `m_pickingDescriptorSets.clear()` efface la collection STL sans appeler `dev.freeDescriptorSets()`. Chaque appel fuit `MAX_FRAMES_IN_FLIGHT` sets.
- **Preuve technique :** Ligne 1068 de `Renderer.cpp`.
- **Statut Double Check :** [x] CONFIRMÉ

### Bug B5 : `waitIdle()` et créations lourdes en plein Command Buffer
- **Bug ID / Signalement :** B5 dans `tasks/CODE_REVIEW.md` (`Renderer.cpp:461-465, 1092-1115`)
- **Diagnostic critique indépendant :** `Renderer::render()` démarre l'enregistrement du command buffer en ligne 449 (`cb.begin()`). En ligne 463, si `m_pickingRequested` est vrai, `renderEntityIds()` est exécuté. Dans `renderEntityIds()`, si le pipeline n'existe pas ou si `targetW != m_pickingWidth`, `createPickingResources()` est exécuté et appelle `dev.waitIdle()`. Appeler `waitIdle()` pendant qu'un command buffer est en cours d'enregistrement provoque un blocage complet du pipeline Vulkan.
- **Preuve technique :** Lignes 449, 463 et 1106 de `Renderer.cpp`.
- **Statut Double Check :** [x] CONFIRMÉ

### Bug B6 : `waitIdle()` global et contention sur `m_commandPool` lors du readback
- **Bug ID / Signalement :** B6 dans `tasks/CODE_REVIEW.md` (`Renderer.cpp:1217-1259`)
- **Diagnostic critique indépendant :** `readEntityIdAt()` alloue un `vk::CommandBuffer` depuis `m_commandPool` (le pool primaire de la frame de rendu) puis appelle `m_context.getGraphicsQueue().waitIdle()`. Cela bloque le thread principal et vide l'ensemble des travaux GPU en cours, causant des à-coups perceptibles à chaque clic de picking.
- **Preuve technique :** Lignes 1224-1225 et 1246 de `Renderer.cpp`.
- **Statut Double Check :** [x] CONFIRMÉ

---

## 2. Découpage en Tâches Atomiques (Approche TDD)

### Tâche 1 : Initialisation Eager & Redimensionnement Dédié (B4 & B5)
- **Fichiers modifiés / créés :** `include/bb3d/render/Renderer.hpp`, `src/bb3d/render/Renderer.cpp`
- **Actions :**
  - Initialiser les ressources de picking (pipeline, instance buffers, descriptor sets, command pool/fence de readback) de manière *eager* dans le constructeur `Renderer::Renderer()`.
  - Extraire `resizePickingImages(uint32_t width, uint32_t height)` qui ne recrée que les images et image views Vulkan/VMA, sans toucher aux descriptor sets.
  - Exécuter `resizePickingImages()` dans le bloc `m_resizeRequested` de `Renderer::render()` où `waitIdle()` est déjà appelé en toute sécurité hors de `cb.begin()`.
  - Supprimer tout `waitIdle()` et toute recréation paresseuse de pipeline de `renderEntityIds()`.
  - Si `createPickingResources()` est réinvoqué, libérer explicitement `dev.freeDescriptorSets(m_descriptorPool, m_pickingDescriptorSets)`.

### Tâche 2 : Readback Isolé par Fence & Transience (B6 & Q15)
- **Fichiers modifiés / créés :** `include/bb3d/render/Renderer.hpp`, `src/bb3d/render/Renderer.cpp`
- **Actions :**
  - Définir `static constexpr uint32_t kPickingNoEntity = 0xFFFFFFFF;`.
  - Créer `m_pickingCommandPool` (`eTransient | eResetCommandBuffer`) et `m_pickingFence` à l'initialisation du picking.
  - Dans `readEntityIdAt()` : allouer/enregistrer la copie 1-pixel dans `m_pickingCommandBuffer`, soumettre avec `m_pickingFence`, et attendre `dev.waitForFences(1, &m_pickingFence, VK_TRUE, UINT64_MAX)`.
  - Remplacer `m_graphicsQueue.waitIdle()` par l'attente de fence ciblée.
  - Détruire proprement le pool, la fence et les buffers dans `cleanupPickingResources()` / `~Renderer()`.

### Tâche 3 : Test Unitaire et Validation (TDD)
- **Fichiers modifiés / créés :** `tests/unit_test_26_picking.cpp`
- **Actions :**
  - Valider l'état prêt (`hasPickingBuffer()`) dès l'initialisation.
  - Valider que `readEntityIdAt()` retourne `kPickingNoEntity` sur les coordonnées invalides.
  - Tester des redimensionnements consécutifs pour confirmer l'absence de saturation du pool de descripteurs.
  - Valider la suite CTest complète.

---

## 3. Grille de Revue & Checkpoints

### 🛠️ Checkpoints de l'Implémenteur (Avant soumission en revue)
- [x] **Double Check Bug (si correctif) :** Section 1.bis dûment remplie, diagnostic critique confirmé pour B4, B5, B6.
- [x] **TDD & Tests :** `unit_test_26_picking.cpp` créé et passant (`ctest -R unit_test_26_picking` PASS).
- [x] **Standards C++ (`cpp-pro`) :**
  - [x] Zéro allocation dynamique dans le *Hot Path* (`render()` / `renderEntityIds()`).
  - [x] Pas de fuite de mémoire CPU / smart pointers `Scope<T>` et RAII.
  - [x] Code et commentaires rédigés en anglais.
- [x] **Standards Vulkan (`vulkan-cpp`) :**
  - [x] Zéro appel bloquant `dev.waitIdle()` ou `queue.waitIdle()` pendant l'enregistrement de command buffer ou par requête de picking.
  - [x] Synchronisation moderne : `pipelineBarrier2` avec `vk::DependencyInfo` et `vk::ImageMemoryBarrier2` dans `renderEntityIds()`.
  - [x] Synchronisation par fence isolée (`m_pickingFence`).
  - [x] Zéro fuite de `vk::DescriptorSet` lors des redimensionnements (`dev.freeDescriptorSets` appelé, buffers/sets non réalloués inutilement).
  - [x] Command Pool transitoire dédié pour les opérations de readback.
- [x] **Qualité du Build :** Zéro warning compilateur sous MSVC (`/W4`).
- [x] **Commits :** Commits atomiques avec messages explicites.

---

### 🔍 Checkpoints des Reviewers (Revue de Code Systématique & Approbation)
- [x] **Architecture & Opacité (`AGENTS.md`) :**
  - [x] Les types Vulkan (`vk::*`) restent 100% opaques vis-à-vis du client (`PickingSystem` utilise `Renderer` sans toucher à Vulkan).
  - [x] Multi-streams sommets préservé pour le picking (seul le stream position 0 est lié au pipeline).
- [x] **Sécurité & Robustesse :**
  - [x] Coordonnées hors limites protégées (`kPickingNoEntity`).
  - [x] Destruction propre et ordonnée dans `~Renderer()` / `cleanupPickingResources()`.
- [x] **Validation GPU & Profiling :**
  - [x] Tests CTest au vert (`unit_test_25_material_frame` et `unit_test_26_picking` PASS).
- [x] **Décision Reviewer :**
  - [x] **APPROVED** (Prêt pour la fusion)
  - [ ] **CHANGES REQUESTED**
- [x] **Historique :** Entrée compacte consignée dans `tasks/HISTORY.md` et mise à jour de `tasks/ROADMAP.md`.

---

## 4. Journal des Échanges & Retours de Revue
- *2026-09-17* - **@Antigravity** : Fiche initialisée avec double-check technique confirmé pour B4, B5 et B6. Mise en place du live-monitoring streaming pour Vibe CLI.
- *2026-09-17* - **@Antigravity** : Implémentation TDD de `createPickingResources` eager, `resizePickingImages` propre, élimination du `waitIdle` mid-frame, synchronisation fence isolée pour `readEntityIdAt`, constante `kPickingNoEntity`, et transition vers `pipelineBarrier2`.
- *2026-09-17* - **@bb3d-reviewer** : Revue de code effectuée. Opacité de l'API cliente respectée (PickingSystem ne touche à aucun type Vulkan), zéro-allocation dans le hot path validée, fuite B4 éliminée avec libération explicite des sets, suppression du stall B5 vérifiée, et synchronisation par fence B6 validée. Approbation formelle [APPROVED].
