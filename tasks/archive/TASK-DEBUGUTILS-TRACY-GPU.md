# [TÂCHE-GPU-04] : Instrumentation DebugUtils & Profiling Tracy GPU

- **Statut :** DONE
- **Auteur / Implémenteur :** @Antigravity
- **Reviewer(s) :** @bb3d-reviewer (Mistral Vibe CLI / glm-5.2)
- **Branche Git :** `feat/debugutils-tracy-gpu`
- **Date de création :** 2026-09-18

---

## 1. Contexte & Objectif

Dans le cadre du **Jalon 2 (Socle Vulkan 1.3/1.4 Moderne & Synchronisation)**, ce chantier vise à doter le moteur `bb3d` d'outils de diagnostic et de profilage GPU de niveau professionnel :
1. **DebugUtils (`VK_EXT_debug_utils`) :**
   - Activation systématique si disponible ou si validation/debug activé.
   - Balisage RAII des command buffers (`ScopedDebugLabel`) avec couleurs pour RenderDoc et Nsight.
   - Nommage explicite type-safe des ressources Vulkan (`setDebugObjectName`).
2. **Profiling Tracy GPU (`TracyVulkan.hpp`) :**
   - Context GPU Tracy `TracyVkCtx` lié à la file graphique.
   - Balisage des passes majeures (`Shadow Pass`, `Skybox Pass`, `Scene PBR`, `Composite Pass`, `Picking Pass`).
   - Collecte automatique des queries GPU (`TracyVkCollect`) à chaque frame.
3. **Zéro fuite d'abstraction :**
   - L'API publique cliente reste 100% opaque (pas de header Vulkan ni Tracy exposé).
4. **Validation TDD :**
   - Nouveau test unitaire `tests/unit_test_34_debug_utils_profiling.cpp` validé sous CTest avec validation layers actives.

---

## 2. Découpage en Tâches Atomiques (Approche TDD)

### Tâche 1 : Infrastructure DebugUtils dans `VulkanContext`
- **Fichiers modifiés / créés :** `include/bb3d/render/VulkanContext.hpp`, `src/bb3d/render/VulkanContext.cpp`, `include/bb3d/render/DebugUtils.hpp`
- **Test unitaire associé :** `tests/unit_test_34_debug_utils_profiling.cpp` (Partie 1 & 2 : extension check, `setObjectName`, `cmdBeginDebugLabel`/`cmdEndDebugLabel`)
- **Étape 1 (Test) :** Écrire le test unitaire `unit_test_34` (RED).
- **Étape 2 (Vérification échec) :** `cmake --build build && ctest -R unit_test_34` (RED).
- **Étape 3 (Code minimal) :** Implémenter la détection de l'extension, `ScopedDebugLabel`, et `setDebugObjectName`.
- **Étape 4 (Vérification succès) :** `cmake --build build && ctest -R unit_test_34` (PASS).
- **Étape 5 (Commit) :** `git commit -m "feat(render): add Vulkan DebugUtils labeling and object naming"`

### Tâche 2 : Intégration Tracy GPU dans `Renderer`
- **Fichiers modifiés / créés :** `include/bb3d/render/Renderer.hpp`, `src/bb3d/render/Renderer.cpp`, `include/bb3d/core/Core.hpp`
- **Test unitaire associé :** `tests/unit_test_34_debug_utils_profiling.cpp` (Partie 3 : Tracy GPU init, zone recording, collect)
- **Étape 1 (Test) :** Étendre le test `unit_test_34` avec une frame mock instrumentée.
- **Étape 2 (Vérification échec) :** Compiler et tester (RED).
- **Étape 3 (Code minimal) :** Initialiser `TracyVkCtx`, ajouter les zones `BB_GPU_ZONE` sur `Shadow Pass`, `Skybox`, `Scene PBR`, `Composite`, `Picking`, et appeler `TracyVkCollect`.
- **Étape 4 (Vérification succès) :** `cmake --build build && ctest -R unit_test_34` (PASS).
- **Étape 5 (Commit) :** `git commit -m "feat(profile): integrate Tracy GPU profiling and pass labeling"`

### Tâche 3 : Validation Complète & Revue
- **Étape 1 :** Lancer la suite CTest complète (16/16 tests).
- **Étape 2 :** Lancer la revue de code Mistral Vibe CLI (`bb3d-reviewer`).
- **Étape 3 :** Traiter immédiatement les remarques / recommandations.
- **Étape 4 :** Clôturer la tâche dans `tasks/HISTORY.md` et archiver.

---

## 3. Grille de Revue & Checkpoints

### 🛠️ Checkpoints de l'Implémenteur (Avant soumission en revue)
- [x] **Double Check Bug (si correctif) :** N/A (Nouvelle fonctionnalité).
- [x] **TDD & Tests :** `unit_test_34_debug_utils_profiling` passe ainsi que tous les 15 autres tests automatisés CTest (16/16 PASS).
- [x] **Standards C++ (`cpp-pro`) :**
  - [x] Zéro allocation dynamique dans le *Hot Path* (`render()` / `update()`).
  - [x] `std::string_view` et `std::array` utilisés pour les labels et les couleurs.
  - [x] Initialisation désignée C++20 (`Type{.field = val}`).
  - [x] `[[nodiscard]]` présent sur les accesseurs.
  - [x] Code, commentaires et logs en **anglais**.
- [x] **Standards Vulkan (`vulkan-cpp`) :**
  - [x] `VK_EXT_debug_utils` géré avec grâce : actif si supporté, no-op sinon.
  - [x] Zéro crash si validation layers absentes.
  - [x] `ScopedDebugLabel` RAII garantissant l'équilibre strict `begin`/`end`.
  - [x] `TracyVkContext` proprement initialisé avec un one-time command buffer et détruit à la fermeture.
- [x] **Qualité du Build :** Zéro warning compilateur sous MSVC `/W4`.
- [x] **Commits :** Commits atomiques et messages conformes.

---

## 4. Checkpoints des Reviewers (Revue de Code Systématique & Approbation)
- [x] **Architecture & Opacité (`AGENTS.md`) :**
  - [x] Headers clients (`Engine`, `Scene`) 100% exempts de symboles Vulkan ou Tracy.
- [x] **Sécurité & Robustesse :**
  - [x] Parité des labels de debug vérifiée.
  - [x] Collecte Tracy sécurisée hors conditions de course.
  - [x] Macro `BB_GPU_ZONE` sécurisée contre les contextes nuls (`(tracyCtx) != nullptr`).
  - [x] Hot-path zero-allocation (`stackBuf[128]` et `kCascadeLabels`).
- [x] **Validation GPU & Profiling :**
  - [x] Validation Layers Khronos : 0 erreur, 0 warning.
  - [x] 16/16 tests CTest unitaires automatisés validés à 100% (PASS).
- [x] **Décision Reviewer :**
  - [x] **APPROVED** (Prêt pour la fusion)
  - [ ] **CHANGES REQUESTED**
- [x] **Historique :** 1 entrée compacte consignée dans `tasks/HISTORY.md`.

---

## 5. Journal des Échanges & Retours de Revue
- *2026-09-18* - **@Antigravity** : Initialisation de la fiche de tâche et du design doc.
- *2026-09-26* - **@bb3d-reviewer** : Revue initiale `[CHANGES REQUESTED]` (sécurisation pointeur nul `BB_GPU_ZONE`, élimination allocations tas dans `cmdBeginDebugLabel` et `renderShadows`).
- *2026-09-26* - **@Antigravity** : Application immédiate des 3 correctifs (buffer de pile 128B et `kCascadeLabels`).
- *2026-09-26* - **@bb3d-reviewer** : Re-vérification formelle, build 0 warning, suite CTest 16/16 PASS. Décision finale : `[APPROVED]`.
