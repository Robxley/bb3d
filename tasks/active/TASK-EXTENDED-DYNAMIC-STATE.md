# [TASK-EXTENDED-DYNAMIC-STATE] : Chantier 6 — Extended Dynamic State (Vulkan 1.3 Core)

- **Statut :** IN PROGRESS
- **Auteur / Implémenteur :** Agent Implémenteur
- **Reviewer(s) :** Agent Vulkan Architecture Reviewer
- **Branche Git :** `feat/extended-dynamic-state`
- **Date de création :** 2026-09-26

---

## 1. Contexte & Objectif

Activer `VK_EXT_extended_dynamic_state` (promu Vulkan 1.3 Core) pour rendre dynamiques les états de pipeline `CullMode`, `FrontFace`, `DepthTestEnable`, `DepthWriteEnable`, `DepthCompareOp` et `PrimitiveTopology`.

**Bénéfice :** Réduction des permutations de pipelines compilées. Les variantes actuelles (cullMode=None pour skybox/shadow, depthWrite=false pour transparents) deviennent des appels runtime sans recompilation.

**Critères d'acceptation :**
1. `EnabledFeatures::extendedDynamicState` activé si supporté par le hardware.
2. `GraphicsPipeline` déclare 9 états dynamiques au lieu de 3 si la feature est active.
3. `Renderer` binde les états dynamiques dans les command buffers.
4. Tests CTest 100% verts (unit_test_37 PASS).

---

## 2. Découpage en Tâches Atomiques (Approche TDD)

### Tâche 1 : Test TDD (RED)
- **Fichier créé :** `tests/unit_test_37_extended_dynamic_state.cpp`
- Écrire le test qui vérifie l'activation de la feature et la non-régression des pipelines.

### Tâche 2 : VulkanContext — EnabledFeatures + activation
- **Fichiers modifiés :** `include/bb3d/render/VulkanContext.hpp`, `src/bb3d/render/VulkanContext.cpp`
- Ajouter `extendedDynamicState` à `EnabledFeatures`.
- Chaîner `vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT` dans les deux branches (Vulkan 1.3 & 1.4).

### Tâche 3 : GraphicsPipeline — Dynamic States étendus
- **Fichier modifié :** `src/bb3d/render/GraphicsPipeline.cpp`
- Si `extendedDynamicState` est disponible dans le contexte, déclarer 9 états dynamiques et ignorer les states statiques correspondants.
- `createPipeline()` reçoit `bool extendedDynamicState` supplémentaire.

### Tâche 4 : Renderer — Binding des états dynamiques
- **Fichier modifié :** `src/bb3d/render/Renderer.cpp`
- Dans `drawScene()`, binder `setCullMode`, `setFrontFace`, `setDepthTestEnable`, `setDepthWriteEnable`, `setDepthCompareOp`, `setPrimitiveTopology` à chaque changement de pipeline.
- Dans `renderShadows()`, binder `setCullMode(eNone)`, `setDepthTestEnable(true)`, `setDepthWriteEnable(true)`, `setDepthCompareOp(eLess)`.

---

## 3. Grille de Revue & Checkpoints

### 🛠️ Checkpoints de l'Implémenteur

- [ ] **Performance Moteur 3D (Règle 0) :**
  - [ ] États dynamiques bindings uniquement lors des changements de pipeline (no redundant calls).
  - [ ] Aucune allocation dans le hot path.
- [ ] **TDD & Tests :** `unit_test_37` PASS + 100% tests existants verts.
- [ ] **Standards C++ (`cpp-pro`) :**
  - [ ] `static constexpr std::array<vk::DynamicState, N>` pour les états dynamiques.
  - [ ] Code et commentaires en anglais.
- [ ] **Standards Vulkan (`vulkan-cpp`) :**
  - [ ] `StructureChain` correct pour l'activation hardware.
  - [ ] Fallback gracieux si `extendedDynamicState` non supporté.
  - [ ] Aucun appel `setCullMode` etc. si feature non activée.
- [ ] **Qualité Build :** Zéro warning MSVC.

### 🔍 Checkpoints des Reviewers

- [ ] **Audit de Performance :** Bindings redondants évités (état courant tracké).
- [ ] **Architecture & Opacité :** Types Vulkan opaques, feature flag correctement propagé.
- [ ] **Robustesse :** Mode dégradé (extendedDynamicState=false) testé et sans crash.
- [ ] **Décision Reviewer :** [ ] **APPROVED** / [ ] **CHANGES REQUESTED**

---

## 4. Journal des Échanges & Retours de Revue
