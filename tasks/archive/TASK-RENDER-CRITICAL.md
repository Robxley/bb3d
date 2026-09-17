# [TASK-RENDER-CRITICAL] : Sécurité de la boucle de rendu (B1, B2, B3)

- **Statut :** [DONE]
- **Auteur / Implémenteur :** @bb3d-fixer
- **Reviewer(s) :** @bb3d-reviewer, @Antigravity
- **Branche Git :** `fix/render-critical-b1-b2-b3`
- **Date de création :** 2026-09-17

---

## 1. Contexte & Objectif

Trois bugs HIGH dans la boucle de rendu du `Renderer` menaçaient la stabilité d'exécution : un accès hors-limites sur les sémaphores au resize (B1), un deadlock de fence si le submit échoue (B2), et un offset d'instances d'ombres incorrect en présence de trous (B3).

**Objectif :**
1. Revalider de façon critique l'existence des 3 bugs dans le code source actuel (Double Check).
2. Corriger les 3 anomalies avec un impact minimal.
3. Valider la suite CTest sans régression.

---

## 1.bis Revalidation Critique du Bug (Obligatoire - Double Check)

### B1 — Sémaphores de rendu non reconstruits au resize
- **Bug ID / Signalement :** B1 dans `tasks/CODE_REVIEW.md`
- **Diagnostic critique indépendant :** `Renderer.cpp:130-131` dimensionnait `m_renderFinishedSemaphores` à `getImageCount()` à l'init. Le bloc resize (`onResize`) ne réassignait que `m_imagesInUseFences`. Si `recreate()` produisait un nombre d'images différent (bascule plein écran, DPI), `m_renderFinishedSemaphores[imageIndex]` devenait un accès hors limites (UB/crash).
- **Statut Double Check :** [x] CONFIRMÉ (Bug réel et latent)

### B2 — Deadlock de fence si submit échoue
- **Bug ID / Signalement :** B2 dans `tasks/CODE_REVIEW.md`
- **Diagnostic critique indépendant :** `Renderer.cpp:433` appelait `resetFences` avant le `submit`. Si `submit` levait une exception, le `catch` déclenchait `onResize` sans re-signaler la fence. À la frame suivante, `waitForFences(..., UINT64_MAX)` bloquait indéfiniment la boucle de rendu.
- **Statut Double Check :** [x] CONFIRMÉ (Bug réel)

### B3 — Offset d'instances d'ombres incorrect
- **Bug ID / Signalement :** B3 dans `tasks/CODE_REVIEW.md`
- **Diagnostic critique indépendant :** `Renderer.cpp:948-953`. Quand un non-caster était sauté (`!cmd.castShadows`), `flushShadowBatch()` était appelé mais `lastMesh` n'était pas réinitialisé. Le prochain caster de même mesh ne déclenchait pas `cmd.mesh != lastMesh` → `batchStart` conservait l'ancien offset avant le trou, décalant les matrices de transformation des ombres.
- **Statut Double Check :** [x] CONFIRMÉ (Bug réel)

---

## 2. Découpage en Tâches Atomiques (Approche TDD)

### Tâche 1 : Reconstruction des sémaphores au resize (B1)
- **Code minimal :** Dans `Renderer::render()`, lors du `m_resizeRequested`, destruction propre des anciens sémaphores et recréation de `m_renderFinishedSemaphores` adaptée au nouveau `m_swapChain->getImageCount()`.

### Tâche 2 : Réordonner / Re-signaler les fences en cas d'exception (B2)
- **Code minimal :** Dans le `catch (...)` de `Renderer::submitAndPresent()`, destruction et recréation de la fence `m_inFlightFences[m_currentFrame]` en état `eSignaled` pour éviter le blocage de la frame suivante.

### Tâche 3 : Réinitialiser lastMesh sur le saut de non-caster (B3)
- **Code minimal :** Dans `Renderer::renderShadows()`, réinitialisation `lastMesh = nullptr;` sur le chemin `!cmd.castShadows` pour forcer un redémarrage de batch propre après chaque interruption.

---

## 3. Grille de Revue & Checkpoints

### 🛡️ Checkpoints de l'Implémenteur (Avant soumission en revue)
- [x] **Double Check Bug :** Les 3 bugs (B1, B2, B3) ont été vérifiés et confirmés de manière critique dans le code source live.
- [x] **TDD & Tests :** `unit_test_03_swapchain` et `unit_test_shadows` compilent et passent.
- [x] **Standards C++ (`cpp-pro`) :** Zéro allocation dynamique dans le hot path, code et commentaires en anglais technique.
- [x] **Standards Vulkan (`vulkan-cpp`) :** Pas de fuite de sémaphores ou fences (destruction propre avant recréation).
- [x] **Qualité du Build :** Zéro warning compilateur sous MSVC (`/W4`).

---

### 🔍 Checkpoints des Reviewers (Revue de Code Systématique & Approbation)
- [x] **Double Check Validé :** Section 1.bis complétée et vérifiée pour B1, B2 et B3.
- [x] **Architecture & Opacité (`AGENTS.md`) :**
  - [x] Pas de fuite de types Vulkan dans les interfaces publiques.
  - [x] Symétrie parfaite destruction/création des objets de synchronisation.
- [x] **Sécurité & Robustesse :**
  - [x] La boucle de rendu ne peut plus geler sur échec de submit (fence garantie signalée).
  - [x] Le redimensionnement swapchain garantit un tableau de sémaphores strictement aligné sur `imageCount`.
  - [x] Les ombres gèrent correctement les listes mixtes de casters et non-casters.
- [x] **Validation CTest :** CTest valide (`unit_test_03_swapchain`, `unit_test_shadows`).
- [x] **Décision Reviewer :** [x] APPROVED
- [x] **Historique :** Consigné dans `tasks/HISTORY.md`.

---

## 4. Journal des Échanges & Retours de Revue
- *2026-09-17* - **@bb3d-fixer** : Ébauche des fiches de tâches et premier diagnostic critique des anomalies.
- *2026-09-17* - **@Antigravity & @bb3d-reviewer** : **Revue croisée conjointe — APPROVED**. Les correctifs pour B1 (sémaphores), B2 (deadlock de fence) et B3 (batching d'ombres) ont été vérifiés, compilés et validés sans aucun impact néfaste sur les performances de la boucle de rendu.

