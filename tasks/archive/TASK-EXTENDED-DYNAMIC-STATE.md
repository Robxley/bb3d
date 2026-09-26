# [TASK-EXTENDED-DYNAMIC-STATE] : Chantier 6 — Extended Dynamic State (Vulkan 1.3 Core)

- **Statut :** ✅ APPROVED
- **Auteur / Implémenteur :** Agent Implémenteur
- **Reviewer(s) :** Agent Vulkan Architecture Reviewer (Antigravity)
- **Branche Git :** `feat/extended-dynamic-state`
- **Date de création :** 2026-09-26
- **Date de revue :** 2026-09-26

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

- [x] **Performance Moteur 3D (Règle 0) :**
  - [x] États dynamiques bindings uniquement lors des changements de pipeline (no redundant calls).
  - [x] Aucune allocation dans le hot path.
- [x] **TDD & Tests :** `unit_test_37` PASS (3.79s) + 100% tests existants non-régressifs.
- [x] **Standards C++ (`cpp-pro`) :**
  - [x] `static constexpr std::array<vk::DynamicState, N>` pour les états dynamiques.
  - [x] Code et commentaires en anglais.
- [x] **Standards Vulkan (`vulkan-cpp`) :**
  - [x] `StructureChain` correct pour l'activation hardware (Vulkan 1.3 et 1.4).
  - [x] Fallback gracieux si `extendedDynamicState` non supporté.
  - [x] Aucun appel `setCullMode` etc. si feature non activée.
- [x] **Qualité Build :** Zéro warning MSVC (/W4).

### 🔍 Checkpoints des Reviewers

- [x] **Audit de Performance :** `extDynState` lu une seule fois en tête de `drawScene()`. Binding initial déterministe avant `renderSkybox()`. Bindings par pipeline uniquement. Zéro allocation. `static constexpr std::array`. ✅
- [x] **Architecture & Opacité :** Types Vulkan opaques. `StructureChain` correct dans les deux branches 1.3 et 1.4. Feature flag cohérent. ✅
- [x] **Robustesse :** Fallback `extendedDynamicState=false` garanti. Highlight pipeline vérifié avec `eLineList` (classe LINE correcte). Binding initial couvre la pollution d'état entre passes (fix R3 commit `3d898c9`). ✅
- [x] **Décision Reviewer :** [x] **APPROVED** — R1 faux positif confirmé. R3 corrigé et validé.

---

## 4. Journal des Échanges & Retours de Revue

### 2026-09-26 — Revue Formelle par Agent Reviewer (Antigravity)

- **R1 (Faux positif vérifié)** : Le pipeline `Highlight` est bien créé avec `vk::PrimitiveTopology::eLineList` dès la création (`Renderer.cpp:352`), conforme à la classe topologique LINE et respectant `VUID-vkCmdSetPrimitiveTopology-None-04912`.
- **R3 (Corrigé - commit `3d898c9`)** : Binding initial déterministe ajouté au début de `drawScene()` avant `renderSkybox()`. `extDynState` lu une seule fois en tête de fonction.
- **R2, R4, R5, R6** : Remarques mineures/cosmétiques notées.
- **Verdict Final** : **APPROVED** par consensus. Prêt pour fusion.
