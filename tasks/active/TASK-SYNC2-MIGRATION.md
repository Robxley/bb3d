# [TASK-SYNC2-MIGRATION] : Migration Complète vers Vulkan Synchronization2 (pipelineBarrier2 & DependencyInfo)

- **Statut :** READY FOR CODE REVIEW
- **Auteur / Implémenteur :** @Antigravity
- **Reviewer(s) :** @bb3d-reviewer (Mistral Vibe CLI), @dev
- **Branche Git :** `feat/sync2-pipeline-barrier2`
- **Date de création :** 2026-09-18
- **Date de révision d'architecture :** 2026-09-18
- **Document de Conception :** `tasks/active/2026-09-18-synchronization2-design.md`

---

## 1. Contexte & Objectif

Le Jalon 2, Chantier 1 a initialisé le socle Vulkan 1.4 et activé la fonctionnalité core `synchronization2` dans `VulkanContext`. Ce second chantier a pour but d'éliminer systématiquement les 18 appels obsolètes à `vk::CommandBuffer::pipelineBarrier` (Vulkan 1.0) dans le moteur, conformément aux règles strictes de `AGENTS.md`.

**Objectifs :**
1. Remplacer tous les appels `cb.pipelineBarrier` dans `src/bb3d/render/Renderer.cpp` (14 occurrences) par `cb.pipelineBarrier2(depInfo)` avec `vk::DependencyInfo` et `vk::ImageMemoryBarrier2`.
2. Regrouper (batching) les barrières consécutives d'attachements couleur et profondeur au sein d'un unique appel `pipelineBarrier2` via `std::array<vk::ImageMemoryBarrier2, N>`.
3. Remplacer les pseudo-stages `eTopOfPipe` et `eBottomOfPipe` par des points de synchronisation précis en 64-bit (`vk::PipelineStageFlagBits2`).
4. Moderniser `Texture::transitionLayout` et `Texture::generateMipmaps` dans `src/bb3d/render/Texture.cpp` (4 occurrences) vers Synchronization2.
5. Créer un test unitaire TDD dédié (`tests/unit_test_32_synchronization2.cpp`) validant la conformité et la non-régression avec les validation layers actives.
6. Garantir 0 warning compilateur et 100% de passage de la suite de tests automatisés.

---

## 2. Découpage en Tâches Atomiques (Approche TDD)

### Tâche 1 : Test Unitaire TDD Synchronization2 & Validation Préalable
- **Fichier créé :** `tests/unit_test_32_synchronization2.cpp`
- **CMakeLists.txt :** Enregistrement automatique par glob.
- **Étape 1 (Test) :** Rédiger un test unitaire simulant une transition d'image et batching de barrières avec `pipelineBarrier2`.
- **Étape 2 (Vérification) :** Compiler et valider la détection/exécution.

### Tâche 2 : Migration Synchronization2 dans `Texture.cpp`
- **Fichier modifié :** `src/bb3d/render/Texture.cpp`
- **Étape 1 :** Remplacer `pipelineBarrier` par `pipelineBarrier2` dans `Texture::transitionLayout` (transitions Undefined -> TransferDst et TransferDst -> ShaderReadOnly).
- **Étape 2 :** Remplacer `pipelineBarrier` par `pipelineBarrier2` dans `Texture::generateMipmaps` (boucle de blit mip-level).
- **Étape 3 :** Valider via `unit_test_09_resource_manager` et `unit_test_32_synchronization2`.

### Tâche 3 : Migration Synchronization2 & Batching dans `Renderer.cpp`
- **Fichier modifié :** `src/bb3d/render/Renderer.cpp`
- **Étape 1 :** Migrer et batcher les barrières de rendu principal (RenderTarget offscreen et Swapchain directe).
- **Étape 2 :** Migrer la barrière de présentation dans `submitAndPresent()` (suppression de `eBottomOfPipe`).
- **Étape 3 :** Migrer et batcher les barrières de post-process dans `compositeToSwapchain()`.
- **Étape 4 :** Migrer les barrières de Shadow Maps dans `renderShadows()`.
- **Étape 5 :** Migrer les barrières de capture d'écran dans `saveScreenshot()` / `copyImageToBuffer()`.

### Tâche 4 : Validation Globale & Suite de Tests
- **Vérification :** `cmake --build build --config Debug` (0 warning, 0 erreur).
- **CTest :** Validation de tous les tests graphiques et non-interactifs (100% PASS).

---

## 3. Grille de Revue & Checkpoints

### 🛠️ Checkpoints de l'Implémenteur (Avant soumission en revue)
- [x] **Standards Vulkan Modernes :** Aucune occurrence résiduelle de `pipelineBarrier` (Vulkan 1.0) dans `Renderer.cpp` ou `Texture.cpp`.
- [x] **Batching Réalisé :** Barrières couleur + profondeur regroupées dans un unique `vk::DependencyInfo`.
- [x] **Masques 64-bit :** Utilisation stricte de `vk::PipelineStageFlagBits2` et `vk::AccessFlagBits2`.
- [x] **Suppression de TopOfPipe / BottomOfPipe :** Remplacés par des étapes exactes.
- [x] **TDD & Tests :** `unit_test_32_synchronization2` et l'ensemble de la suite de tests unitaires passent à 100%.
- [x] **Qualité du Build :** Zéro warning compilateur, logs et commentaires en anglais technique.

---

### 🔍 Checkpoints des Reviewers (Revue d'Architecture & Code Review)
- [ ] **Architecture :** L'élimination des 18 barrières legacy est complète et le batching améliore l'efficacité des soumissions.
- [ ] **Robustesse GPU :** Aucun deadlock, validation layers Khronos sans avertissement ni erreur.
- [ ] **Décision Reviewer :** [ ] APPROVED | [ ] CHANGES REQUESTED

---

## 4. Journal des Échanges & Retours de Revue
- *2026-09-18* - **@Antigravity** : Création du document de conception et de la fiche de tâche pour le Chantier 2 du Jalon 2.
