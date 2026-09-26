# [TÂCHE-HOTPATH-RENDERER-OPTIM] : Optimisation Hot-Path Renderer (Points 1, 2 et 3)

- **Statut :** APPROVED
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
- [x] **Double Check Bug / Problématique :** Diagnostic confirmé et mesuré (96 octets ➔ 24 octets, suppression d'erase vectoriel).
- [x] **Performance Moteur 3D & Jeu Vidéo (Règle 0) :**
  - [x] Architecture pensée pour le frametime temps réel (60+ FPS stables, latence minimale).
  - [x] Structures de données compactes, contiguës et respectueuses des lignes de cache L1/L2/L3 (Data-Oriented Design).
  - [x] Minimisation de la bande passante mémoire et élimination des indirections/copies superflues.
- [x] **TDD & Tests :** Nouveau test unitaire 36 et tests non interactifs passent (`ctest`).
- [x] **Standards C++ (`cpp-pro`) :**
  - [x] Zéro allocation dynamique dans le *Hot Path* (`render()` / `update()`).
  - [x] `std::span` et `std::string_view` utilisés pour le passage de paramètres (Zero-Copy).
  - [x] Initialisation désignée C++20 (`Type{.field = val}`).
  - [x] `[[nodiscard]]` présent sur les accesseurs et fonctions critiques.
  - [x] Code, commentaires, logs et documentation Doxygen rédigés en **anglais**.
- [x] **Standards Vulkan (`vulkan-cpp`) :**
  - [x] Synchronisation moderne : `pipelineBarrier2` avec `vk::DependencyInfo` inchangée et préservée.
  - [x] Buffers persistants mappés sans map/unmap.
  - [x] Dynamic Rendering sans RenderPass legacy.
- [x] **Qualité du Build :** Zéro warning compilateur (`/W4` sous MSVC).
- [x] **Commits :** Commits atomiques et messages de commit clairs (`feat:`).

---

### 🔍 Checkpoints des Reviewers (Revue de Code Systématique & Approbation)
- [x] **Audit de Performance Moteur 3D & Jeu Vidéo (Règle 0) :**
  - [x] Zéro copie masquée ni allocation cachée.
  - [x] Pas de dispatch virtuel ou d'indirection superflue sur le chemin critique.
  - [x] Absence d'impact négatif sur le frametime, la latence CPU/GPU ou la bande passante mémoire.
- [x] **Architecture & Opacité (`AGENTS.md`) :**
  - [x] Les types Vulkan (`vk::*`) restent 100% opaques vis-à-vis du code utilisateur/scene.
  - [x] Respect de la séparation CPU/GPU (pas de transfert inutile par frame).
- [x] **Sécurité & Robustesse :**
  - [x] Vérifications lourdes correctement isolées sous `#if defined(BB3D_DEBUG)`.
  - [x] Gestion des cas limites (zéro entité, 10000+ entités plafonnées à `MAX_INSTANCES`).
- [x] **Décision Reviewer :**
  - [x] **APPROVED** (Prêt pour la fusion)
  - [ ] **CHANGES REQUESTED** (Voir commentaires ci-dessous)
- [x] **Historique :** 1 entrée compacte consignée dans `tasks/HISTORY.md` (à archiver lors du merge).

---

## 4. Journal des Échanges & Retours de Revue
- *2026-09-26* - **@Antigravity** : Initialisation de la fiche de tâche et du design doc après validation de l'Approche 3 par l'utilisateur.
- *2026-09-26* - **@Vulkan Architecture Reviewer** : Revue formelle et indépendante du diff `main...feat/hotpath-renderer-optimization`.
  - Structure `RenderCommand` compactée à 24 octets réels vérifiée (bitfields `transformIndex : 31`, `castShadows : 1`, alignement 8, `static_assert`).
  - Tri `std::ranges::sort` ultra-rapide sur 24 octets (3 mots 64-bit déplaçables via registres, préservation du cache L1D CPU).
  - Élimination complète de `std::remove_if` et `erase` vectoriel grâce au Frustum Culling amont (pré-insertion).
  - Streaming vectorisé contigu vers le buffer mappé `dst[i] = m_transforms[cmd.transformIndex]`.
  - Zéro allocation dynamique en hot-path (vecteurs pré-réservés à 1000 éléments).
  - Gestion rigoureuse des cas limites (mesh nul, caméra nulle, scène vide, saturation MAX_INSTANCES, casters d'ombres hors frustum dans la portée).
  - **Décision : [APPROVED]** sans réserve.
