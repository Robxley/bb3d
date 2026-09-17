# [TÂCHE-XXX] : [Nom de la Fonctionnalité ou Correctif]

- **Statut :** [DRAFT | READY FOR ARCHITECTURE REVIEW | APPROVED | IN PROGRESS | READY FOR CODE REVIEW | CHANGES REQUESTED | DONE]
- **Auteur / Implémenteur :** @nom_ou_agent
- **Reviewer(s) :** @nom_ou_agent
- **Branche Git :** `feat/nom-de-branche` ou `fix/...`
- **Date de création :** AAAA-MM-JJ

---

## 1. Contexte & Objectif
<!-- Description concise du besoin, des fonctionnalités attendues et des critères d'acceptation. -->

---

## 1.bis Revalidation Critique du Bug (Obligatoire si Tâche de type Bug Fix)
<!-- Si la tâche traite un bug rapporté (CODE_REVIEW.md ou retour de revue), l'agent fixeur DOIT vérifier de manière critique son existence dans le code actuel (Double Check). -->
- **Bug ID / Signalement :** [ex: B8 dans tasks/CODE_REVIEW.md]
- **Diagnostic critique indépendant :** [L'agent fixeur décrit son analyse du code source live : confirmation ou réfutation du bug]
- **Preuve technique / Scénario de panne :** [Fichier, ligne exacte, conditions de déclenchement]
- **Statut Double Check :** [ ] CONFIRMÉ (Bug réel et reproductible) / [ ] RÉFUTÉ (Faux positif argumenté)

---

## 2. Découpage en Tâches Atomiques (Approche TDD)

### Tâche 1 : [Composant / Fichier]
- **Fichiers modifiés / créés :** `include/bb3d/...`, `src/bb3d/...`, `tests/...`
- **Test unitaire associé :** `tests/unit_test_xxx.cpp`
- **Étape 1 (Test) :** Écrire le test unitaire qui échoue initialement (reproduction du bug ou nouveau comportement).
- **Étape 2 (Vérification échec) :** `cmake --build build && ctest -R test_xxx` (RED).
- **Étape 3 (Code minimal) :** Implémenter le code source minimal nécessaire.
- **Étape 4 (Vérification succès) :** `cmake --build build && ctest -R test_xxx` (PASS / GREEN).
- **Étape 5 (Commit) :** `git commit -m "feat/fix: description concise"`

---

## 3. Grille de Revue & Checkpoints

### 🛠️ Checkpoints de l'Implémenteur (Avant soumission en revue)
- [ ] **Double Check Bug (si correctif) :** L'existence du bug a été vérifiée de manière critique et confirmée dans le code source avant toute modification (pas de faux positif).
- [ ] **TDD & Tests :** Les nouveaux tests et les tests unitaires existants passent (`ctest`).
- [ ] **Standards C++ (`cpp-pro`) :**
  - [ ] Zéro allocation dynamique dans le *Hot Path* (`render()` / `update()`).
  - [ ] `std::span` et `std::string_view` utilisés pour le passage de paramètres (Zero-Copy).
  - [ ] Initialisation désignée C++20 (`Type{.field = val}`).
  - [ ] `[[nodiscard]]` présent sur les accesseurs et fonctions critiques.
  - [ ] Code, commentaires, logs et documentation Doxygen rédigés en **anglais**.
- [ ] **Standards Vulkan (`vulkan-cpp`) :**
  - [ ] Synchronisation moderne : `pipelineBarrier2` avec `vk::DependencyInfo` (aucun appel à `pipelineBarrier` legacy).
  - [ ] Zéro attente bloquante CPU (`waitIdle()`) pour les uploads.
  - [ ] Aucune fuite de Descriptor Sets (libération RAII vérifiée).
  - [ ] Dynamic Rendering sans RenderPass legacy.
- [ ] **Qualité du Build :** Zéro warning compilateur (`/W4` sous MSVC, `-Wall -Wextra` sous Clang/GCC).
- [ ] **Commits :** Commits atomiques et messages de commit clairs (`feat:`, `fix:`, `refactor:`).

---

### 🔍 Checkpoints des Reviewers (Revue de Code Systématique & Approbation)
- [ ] **Architecture & Opacité (`AGENTS.md`) :**
  - [ ] Les types Vulkan (`vk::*`) restent 100% opaques vis-à-vis du code utilisateur/scene.
  - [ ] Respect de la séparation CPU/GPU (pas de transfert inutile par frame).
  - [ ] Multi-streams sommets respecté (pas d'Uber-Vertex sur les passes d'ombres/picking).
- [ ] **Sécurité & Robustesse :**
  - [ ] Vérifications lourdes correctement isolées sous `#if defined(BB3D_DEBUG)`.
  - [ ] Gestion des cas limites (redimensionnement à 0x0, textures manquantes, etc.).
- [ ] **Validation GPU & Profiling :**
  - [ ] Validation Layers Khronos sans erreur ni warning.
  - [ ] Absence de régressions de framerate ou de pics mémoire inattendus.
- [ ] **Décision Reviewer :**
  - [ ] **APPROVED** (Prêt pour la fusion)
  - [ ] **CHANGES REQUESTED** (Voir commentaires ci-dessous)
- [ ] **Historique :** 1 entrée compacte consignée dans `tasks/HISTORY.md`.

---

## 4. Journal des Échanges & Retours de Revue
- *[Date]* - **@Reviewer** : [Commentaire / suggestion d'amélioration]
- *[Date]* - **@Auteur** : [Correctif appliqué dans le commit hash]
