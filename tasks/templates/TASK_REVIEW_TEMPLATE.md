# [TÂCHE-XXX] : [Nom de la Fonctionnalité ou Correctif]

- **Statut :** [DRAFT | READY FOR REVIEW | IN PROGRESS | CODE REVIEW | DONE]
- **Auteur / Implémenteur :** @nom_ou_agent
- **Reviewer(s) :** @nom_ou_agent
- **Branche Git :** `feat/nom-de-branche` ou `fix/...`
- **Date de création :** AAAA-MM-JJ

---

## 1. Contexte & Objectif
<!-- Description concise du besoin, des fonctionnalités attendues et des critères d'acceptation. -->

---

## 2. Découpage en Tâches Atomiques (Approche TDD)

### Tâche 1 : [Composant / Fichier]
- **Fichiers modifiés / créés :** `include/bb3d/...`, `src/bb3d/...`, `tests/...`
- **Test unitaire associé :** `tests/unit_test_xxx.cpp`
- **Étape 1 (Test) :** Écrire le test unitaire qui échoue initialement.
- **Étape 2 (Vérification échec) :** `cmake --build build && ctest -R test_xxx`
- **Étape 3 (Code minimal) :** Implémenter le code source minimal nécessaire.
- **Étape 4 (Vérification succès) :** `cmake --build build && ctest -R test_xxx` (PASS).
- **Étape 5 (Commit) :** `git commit -m "feat/fix: description concise"`

---

## 3. Grille de Revue & Checkpoints

### 🛠️ Checkpoints de l'Implémenteur (Avant soumission en revue)
- [ ] **TDD & Tests :** Les nouveaux tests et les 29 tests unitaires existants passent (`ctest`).
- [ ] **Standards C++ (`cpp_pro`) :**
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

### 🔍 Checkpoints des Reviewers (Validation & Approbation)
- [ ] **Architecture & Opacité (`GEMINI.md`) :**
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
