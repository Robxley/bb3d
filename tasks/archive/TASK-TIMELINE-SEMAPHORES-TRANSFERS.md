# [TASK-TIMELINE-SEMAPHORES-TRANSFERS] : Timeline Semaphores & Transferts Réellement Asynchrones (B11, P11)

- **Statut :** COMPLETED
- **Auteur / Implémenteur :** @Antigravity
- **Reviewer(s) :** @bb3d-reviewer (Mistral Vibe CLI), @dev
- **Branche Git :** `feat/timeline-semaphores-transfers`
- **Date de création :** 2026-09-18
- **Document de Conception :** `tasks/active/2026-09-18-timeline-semaphores-transfers-design.md`

---

## 1. Contexte & Objectif

L'objectif est d'éliminer les attentes CPU bloquantes (`waitForFences(UINT64_MAX)` et `waitIdle`) lors du chargement des textures et buffers, en introduisant un **Timeline Semaphore** Vulkan 1.3/1.4 dédié à la file de transfert (`m_transferQueue`), avec interrogation non-bloquante et recyclage propre des command buffers. Cette étape pose également les bases pour la synchronisation GPU inter-frames (Approche B ultérieure).

---

## 1.bis Revalidation Critique du Bug (Double Check)

- **Bug ID / Signalement :** `B11` (`VulkanContext.cpp:406`), `P11` (`VulkanContext.cpp:372`).
- **Diagnostic critique indépendant :**
  1. `VulkanContext.cpp:406` : `endTransferCommandsAsync()` appelle directement `(void)m_device.waitForFences(fence, true, std::numeric_limits<uint64_t>::max());` avant de libérer le command buffer et de retourner une fence déjà signalée. Tout chargement de texture fige donc le thread CPU principal.
  2. `Texture.cpp:219-232` : `isReady()` interroge une fence qui a déjà été attendue au moment de la soumission.
  3. `Texture.hpp:59-60` : `Texture` stocke une `vk::Fence` brute nécessitant destruction manuelle.
- **Preuve technique :** Code inspecté et confirmé dans `VulkanContext.cpp` et `Texture.cpp`.
- **Statut Double Check :** [x] CONFIRMÉ (Bug réel et reproductible)

---

## 2. Découpage en Tâches Atomiques (Approche TDD)

### Tâche 1 : Test Unitaire TDD Dédié (`tests/unit_test_33_timeline_transfers.cpp`)
- **Fichiers modifiés / créés :** `tests/unit_test_33_timeline_transfers.cpp`
- **Étape 1 (Test) :** Créer le test unitaire validant :
  1. La validité et l'état initial (0) du timeline semaphore de transfert de `VulkanContext`.
  2. L'incrémentation monotone asynchrone des valeurs retournées par `endTransferCommandsAsync` sans blocage CPU.
  3. La complétion effective des transferts et l'interrogation non-bloquante de `Texture::isReady()`.
- **Étape 2 (Vérification échec) :** Compiler et exécuter pour vérifier la phase RED.

### Tâche 2 : Implémentation du Timeline Semaphore dans `VulkanContext`
- **Fichiers modifiés :** `include/bb3d/render/VulkanContext.hpp`, `src/bb3d/render/VulkanContext.cpp`
- **Étape 1 :** Ajouter `m_transferTimelineSemaphore`, `m_transferTimelineValue` (atomique 64-bit), et la gestion `m_pendingTransfers`.
- **Étape 2 :** Implémenter `endTransferCommandsAsync` soumettant avec `vk::TimelineSemaphoreSubmitInfo` sur `m_transferQueue` sans aucun appel à `waitForFences`.
- **Étape 3 :** Implémenter `getCompletedTransferTimelineValue()`, `waitTransferTimeline()`, et `pollTransferCompletions()`.

### Tâche 3 : Modernisation de `Texture` (Élimination des Fences)
- **Fichiers modifiés :** `include/bb3d/render/Texture.hpp`, `src/bb3d/render/Texture.cpp`
- **Étape 1 :** Remplacer `m_uploadFence` par `uint64_t m_uploadTimelineValue`.
- **Étape 2 :** Adapter `isReady()` pour interroger `getCompletedTransferTimelineValue() >= m_uploadTimelineValue` de manière 100% non-bloquante.
- **Étape 3 :** Nettoyer le destructeur `~Texture()` pour attendre la timeline via `waitTransferTimeline()` au lieu de `destroyFence`.

### Tâche 4 : Validation Globale & Suite CTest
- **Vérification :** Build complet `Debug` sans warning (`/W4`), 100% de tests réussis sur la suite CTest avec validation layers actives.

---

## 3. Grille de Revue & Checkpoints

### 🛠️ Checkpoints de l'Implémenteur (Avant soumission en revue)
- [x] **Double Check Bug :** L'existence de `B11` confirmée dans le code live avant modification.
- [x] **TDD & Tests :** Nouveau test `unit_test_33_timeline_transfers` et l'ensemble des 15 tests unitaires passent à 100%.
- [x] **Standards C++ (`cpp-pro`) :** Zéro allocation dans le hot-path, std::atomic<uint64_t> memory-safe, commentaires en anglais.
- [x] **Standards Vulkan (`vulkan-cpp`) :** Timeline Semaphores natifs Core 1.3/1.4, zéro `waitForFences(UINT64_MAX)` dans les uploads, 0 erreur de validation layer.
- [x] **Qualité du Build :** Zéro warning compilateur MSVC.

---

### 🔍 Checkpoints des Reviewers (Revue de Code Systématique & Approbation)
- [x] **Architecture & Opacité :** L'API publique reste découplée des types Vulkan (`vk::*`).
- [x] **Performance :** Les chargements d'assets ne bloquent plus la boucle principale CPU.
- [x] **Décision Reviewer :** [x] APPROVED | [ ] CHANGES REQUESTED

---

## 4. Journal des Échanges & Retours de Revue
- *2026-09-18* - **@Antigravity** : Création du document de conception (`2026-09-18-timeline-semaphores-transfers-design.md`) et de la fiche de tâche. Approche A validée par l'utilisateur.
- *2026-09-18* - **@Antigravity** : Implémentation complète de l'Approche A : `m_transferTimelineSemaphore` dans `VulkanContext`, `endTransferCommandsAsync` non-bloquant retournant `uint64_t`, recyclage des command buffers, refonte de `Texture::isReady()` sans `vk::Fence`. Test unitaire TDD `unit_test_33_timeline_transfers` (PASS, 0.72s), suite CTest 15/15 PASS (100%), 0 warning MSVC. Soumission en revue de code formelle.
- *2026-09-18* - **@bb3d-reviewer (glm-5.2)** : Revue de code formelle exécutée avec succès (`tasks/active/logs/TASK-TIMELINE-SEMAPHORES-TRANSFERS_reviewer_*.log`). Décision : **[APPROVED]**.
- *2026-09-18* - **@Antigravity** : Application immédiate des 3 recommandations du reviewer : câblage proactif de `pollTransferCompletions()` en début de frame dans `Renderer::render()`, barrières transfer-compatibles dans `unit_test_33`, et simplification des flags du pool de transfert. Suite CTest 15/15 PASS.
