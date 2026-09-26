# [TASK-EDITOR-MODULAR-CORE] : Phase 4.1 — Socle Modulaire & Découpage de l'Éditeur

- **Statut :** IN PROGRESS
- **Auteur / Implémenteur :** Agent Implémenteur & Engine Tools Architect
- **Reviewer(s) :** Agent Vulkan Architecture Reviewer
- **Branche Git :** `feat/editor-modular-core`
- **Date de création :** 2026-09-26

---

## 1. Contexte & Objectif

L'éditeur actuel repose sur un `ImGuiLayer` monolithique de plus de 1100 lignes (`src/bb3d/core/ImGuiLayer.cpp`) couplé à `Engine.cpp`.

**Objectif :**
1. Mettre en place l'architecture modulaire découplée définie dans [`docs/editor/EDITOR_ARCHITECTURE_SPECIFICATION.md`](../../docs/editor/EDITOR_ARCHITECTURE_SPECIFICATION.md) :
   - `EditorContext` (bus d'état, sélection, simulation, pivot barycentrique)
   - `EditorPanel` (interface abstraite de panneau)
   - `EditorPanelManager` (gestionnaire de cycle de vie et d'événements)
2. Appliquer le **Design System Dark Pro** défini dans [`docs/editor/EDITOR_UI_UX_DESIGN_SYSTEM.md`](../../docs/editor/EDITOR_UI_UX_DESIGN_SYSTEM.md) (`EditorStyle::ApplyDarkProTheme()`).
3. Découper `ImGuiLayer.cpp` en 6 panneaux autonomes :
   - `ViewportPanel`
   - `SceneHierarchyPanel`
   - `InspectorPanel`
   - `ToolbarPanel`
   - `SceneSettingsPanel`
   - `ConsolePanel`
4. Résoudre définitivement les bugs statiques **N5**, **N6**, **N7** de `tasks/CODE_REVIEW.md`.

---

## 2. Découpage en Tâches Atomiques (Approche TDD)

### Tâche 1 : Test TDD `EditorContext` & `EditorPanelManager` (RED)
- **Fichier créé :** `tests/unit_test_38_editor_context.cpp`
- Test unitaire vérifiant la sélection, le calcul du centre de sélection, l'ajout/suppression de panneaux et le cycle de vie `onAttach`/`onDetach`.

### Tâche 2 : Implémentation du Socle Central (`EditorContext`, `EditorPanel`, `EditorPanelManager`)
- **Fichiers créés :**
  - `include/bb3d/editor/EditorContext.hpp`, `src/bb3d/editor/EditorContext.cpp`
  - `include/bb3d/editor/EditorPanel.hpp`
  - `include/bb3d/editor/EditorPanelManager.hpp`, `src/bb3d/editor/EditorPanelManager.cpp`
- Validation du test `unit_test_38_editor_context` (PASS / GREEN).

### Tâche 3 : Design System Dark Pro (`EditorStyle`)
- **Fichiers créés :**
  - `include/bb3d/editor/EditorStyle.hpp`, `src/bb3d/editor/EditorStyle.cpp`
- Application des couleurs, arrondis, espacements et styles Dark Pro.

### Tâche 4 : Découpage des Panneaux Autonomes
- **Fichiers créés :**
  - `include/bb3d/editor/panels/SceneHierarchyPanel.hpp`, `src/bb3d/editor/panels/SceneHierarchyPanel.cpp`
  - `include/bb3d/editor/panels/InspectorPanel.hpp`, `src/bb3d/editor/panels/InspectorPanel.cpp`
  - `include/bb3d/editor/panels/ViewportPanel.hpp`, `src/bb3d/editor/panels/ViewportPanel.cpp`
  - `include/bb3d/editor/panels/ToolbarPanel.hpp`, `src/bb3d/editor/panels/ToolbarPanel.cpp`
  - `include/bb3d/editor/panels/SceneSettingsPanel.hpp`, `src/bb3d/editor/panels/SceneSettingsPanel.cpp`
  - `include/bb3d/editor/panels/ConsolePanel.hpp`, `src/bb3d/editor/panels/ConsolePanel.cpp`

### Tâche 5 : Allègement de `ImGuiLayer` & Résolution N5, N6, N7
- `ImGuiLayer` ne gère plus que l'infrastructure (Vulkan/SDL3, fonts, dockspace, menu bar) et délègue au `EditorPanelManager`.
- Correction de N5 (suppression doublon Light), N6 (partCol non statique), N7 (élimination s_loadConfig).
- Mise à jour de `CMakeLists.txt`.

### Tâche 6 : Validation Globale CTest
- Vérification que l'ensemble des tests passent.

---

## 3. Grille de Revue & Checkpoints

### 🛠️ Checkpoints de l'Implémenteur (Avant soumission en revue)
- [x] **Performance Moteur 3D & Jeu Vidéo (Règle 0) :**
  - [x] Zéro allocation dynamique dans le hot path ImGui (`onImGuiRender()` et `onUpdate()`).
  - [x] Panels pré-alloués et stockés de façon contiguë (`std::vector`).
  - [x] Sélection avec capacité réservée à l'avance (`m_selectedEntities.reserve(32)`).
- [x] **TDD & Tests :** `unit_test_38_editor_context` PASS (0.48s) + 100% tests automatisés existants sans régression.
- [x] **Standards C++ (`cpp-pro`) :**
  - [x] `std::string_view` pour les identifiants et titres.
  - [x] `[[nodiscard]]` sur les accesseurs.
  - [x] Code et commentaires en anglais technique.
- [x] **Standards Vulkan (`vulkan-cpp`) & Opacité :**
  - [x] Les panneaux d'édition n'incluent pas d'en-têtes Vulkan `<vulkan/...>`.
  - [x] Les textures pour ImGui sont enregistrées exclusivement via `ImGuiLayer::addTexture`.
- [x] **Qualité du Build :** Zéro warning MSVC (`/W4`).
- [x] **Bugs N5, N6, N7 éradiqués.**

### 🔍 Checkpoints des Reviewers
- [ ] **Audit de Performance :** Zéro allocation cachée, pas de dispatches virtuels superflus dans le hot path de rendu.
- [ ] **Architecture & Opacité :** `EditorPanel` découplé, `EditorContext` propre, zéro variable statique globale résiduelle.
- [ ] **Robustesse :** Scènes vides, entités sans composants, multi-sélection cohérente.
- [ ] **Décision Reviewer :** [ ] **APPROVED** / [ ] **CHANGES REQUESTED**

---

## 4. Journal des Échanges & Retours de Revue
- *2026-09-26* - **@Implémenteur** : Création de la tâche et initialisation du chantier Phase 4.1.
- *2026-09-26* - **@Implémenteur** : Implémentation complète terminée :
  - `EditorContext`, `EditorPanel`, `EditorPanelManager`, `EditorStyle` (Thème Dark Pro UE5/Blender 4).
  - 6 panneaux modulaires créés (`ViewportPanel`, `SceneHierarchyPanel`, `InspectorPanel`, `ToolbarPanel`, `SceneSettingsPanel`, `ConsolePanel`).
  - `ImGuiLayer.cpp` allégé de 1161 lignes à 314 lignes propres délégant à `EditorPanelManager`.
  - Bugs statiques N5 (double if Light), N6 (static partCol), N7 (static s_loadConfig) éradiqués.
  - Tests CTest 100% PASS (unit_test_38 validé en 0.48s). Prêt pour revue de code.
