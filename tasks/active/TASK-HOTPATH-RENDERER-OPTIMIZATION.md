# [TÂCHE-HOTPATH-RENDERER-OPTIM] : Optimisation Hot-Path Renderer (Points 1, 2 et 3)

- **Statut :** IN PROGRESS
- **Auteur / Implémenteur :** @Antigravity
- **Reviewer(s) :** @Vulkan Architecture Reviewer
- **Branche Git :** `feat/hotpath-renderer-optimization`
- **Date de création :** 2026-09-26

---

## 1. Contexte & Objectif

Dans le cadre de l'application stricte de la **Règle Suprême 0 (Performance Moteur 3D Temps Réel)**, optimiser le chemin critique de préparation des commandes de rendu dans `Renderer` :
1. **Point 1 :** Compactage de `RenderCommand` (96 octets ➔ 24 octets) pour accélérer le tri `std::ranges::sort` et préserver le cache L1 CPU.
2. **Point 2 :** Frustum Culling en amont dès la collecte des entités pour éviter l'insertion de commandes culled, éliminant le coût de `std::remove_if` et `m_renderCommands.erase()`.
3. **Point 3 :** Copie optimisée vers l'Instance Buffer GPU mappé (streaming store contigu de `glm::mat4`).

Critères d'acceptation :
- `sizeof(RenderCommand) == 24` vérifié par `static_assert`.
- Zéro régression sur le rendu PBR, les ombres cascadées et le frustum culling.
- Test unitaire TDD automatisé validant la structure compacte, l'alignement et la validité du tri et du buffer d'instances.
- Validation des tests automatisés CTest.

---

## 2. Découpage en Tâches Atomiques (Approche TDD)

### Tâche 1 : Test TDD & Structures Compactes
- **Fichiers modifiés / créés :** `tests/unit_test_36_renderer_hotpath_optim.cpp`, `CMakeLists.txt`
- **Test unitaire associé :** `tests/unit_test_36_renderer_hotpath_optim.cpp`
- **Étape 1 (Test) :** Écrire le test unitaire validant la taille de `RenderCommand` (24 octets), la correspondance des indices de matrices et le bon tri des commandes.
- **Étape 2 (Vérification échec) :** Compiler et exécuter (RED si structure non mise à jour).

### Tâche 2 : Refactorisation de `Renderer.hpp` et `Renderer.cpp`
- **Fichiers modifiés :** `include/bb3d/render/Renderer.hpp`, `src/bb3d/render/Renderer.cpp`
- **Étape 3 (Code minimal) :**
  - Mettre à jour `RenderCommand` dans `Renderer.hpp`.
  - Ajouter `std::vector<glm::mat4> m_transforms` pré-réservé dans `Renderer`.
  - Refondre `Renderer::prepareRenderData()` pour effectuer le Frustum Culling avant insertion, stocker les matrices dans `m_transforms` et streamer les matrices triées vers `mappedData`.
- **Étape 4 (Vérification succès) :** `cmake --build build && ctest -R unit_test_36` (GREEN).
- **Étape 5 (Commit) :** Commit atomique sur `feat/hotpath-renderer-optimization`.

---

## 3. Grille de Revue & Checkpoints

### 🛠️ Checkpoints de l'Implémenteur (Avant soumission en revue)
- [ ] **Double Check Bug / Problématique :** Diagnostic confirmé et mesuré (96 octets ➔ 24 octets, suppression d'erase vectoriel).
- [ ] **Performance Moteur 3D & Jeu Vidéo (Règle 0) :**
  - [ ] Architecture pensée pour le frametime temps réel (60+ FPS stables, latence minimale).
  - [ ] Structures de données compactes, contiguës et respectueuses des lignes de cache L1/L2/L3 (Data-Oriented Design).
  - [ ] Minimisation de la bande passante mémoire et élimination des indirections/copies superflues.
- [ ] **TDD & Tests :** Nouveau test unitaire 36 et tests non interactifs passent (`ctest`).
- [ ] **Standards C++ (`cpp-pro`) :**
  - [ ] Zéro allocation dynamique dans le *Hot Path* (`render()` / `update()`).
  - [ ] `std::span` et `std::string_view` utilisés pour le passage de paramètres (Zero-Copy).
  - [ ] Initialisation désignée C++20 (`Type{.field = val}`).
  - [ ] `[[nodiscard]]` présent sur les accesseurs et fonctions critiques.
  - [ ] Code, commentaires, logs et documentation Doxygen rédigés en **anglais**.
- [ ] **Standards Vulkan (`vulkan-cpp`) :**
  - [ ] Synchronisation moderne : `pipelineBarrier2` avec `vk::DependencyInfo` inchangée et préservée.
  - [ ] Buffers persistants mappés sans map/unmap.
  - [ ] Dynamic Rendering sans RenderPass legacy.
- [ ] **Qualité du Build :** Zéro warning compilateur (`/W4` sous MSVC).
- [ ] **Commits :** Commits atomiques et messages de commit clairs (`feat:`).

---

### 🔍 Checkpoints des Reviewers (Revue de Code Systématique & Approbation)
- [ ] **Audit de Performance Moteur 3D & Jeu Vidéo (Règle 0) :**
  - [ ] Zéro copie masquée ni allocation cachée.
  - [ ] Pas de dispatch virtuel ou d'indirection superflue sur le chemin critique.
  - [ ] Absence d'impact négatif sur le frametime, la latence CPU/GPU ou la bande passante mémoire.
- [ ] **Architecture & Opacité (`AGENTS.md`) :**
  - [ ] Les types Vulkan (`vk::*`) restent 100% opaques vis-à-vis du code utilisateur/scene.
  - [ ] Respect de la séparation CPU/GPU (pas de transfert inutile par frame).
- [ ] **Sécurité & Robustesse :**
  - [ ] Vérifications lourdes correctement isolées sous `#if defined(BB3D_DEBUG)`.
  - [ ] Gestion des cas limites (zéro entité, 10000+ entités plafonnées à `MAX_INSTANCES`).
- [ ] **Décision Reviewer :**
  - [ ] **APPROVED** (Prêt pour la fusion)
  - [ ] **CHANGES REQUESTED** (Voir commentaires ci-dessous)
- [ ] **Historique :** 1 entrée compacte consignée dans `tasks/HISTORY.md`.

---

## 4. Journal des Échanges & Retours de Revue
- *2026-09-26* - **@Antigravity** : Initialisation de la fiche de tâche et du design doc après validation de l'Approche 3 par l'utilisateur.
