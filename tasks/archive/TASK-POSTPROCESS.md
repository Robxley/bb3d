# [TASK-POSTPROCESS] : Correction du Post-Processing — PostProcessUBO non lié (N1)

- **Statut :** DONE
- **Auteur / Implémenteur :** @Antigravity
- **Reviewer(s) :** @bb3d-reviewer, @User
- **Branche Git :** `fix/render-critical-b1-b2-b3`
- **Date de création :** 2026-09-17

---

## 1. Contexte & Objectif

Le shader `copy.frag` déclare un UBO `PostProcessUBO` au binding 1 (exposure, gamma, enableTonemapping, enableGammaCorrection), mais le `DescriptorSetLayout` créé dans `Renderer.cpp` n'enregistre que le binding 0 (sampler `sourceImage`). L'UBO n'est jamais lié → le shader lit des données indéfinies pour `pp.exposure`, `pp.gamma`, etc. Le tonemapping ACES et la correction gamma du post-processing sont **cassés silencieusement**.

**Objectif :**
1. Confirmer le bug dans le code live (Double Check).
2. Ajouter le binding `PostProcessUBO` au `m_copyLayout`.
3. Créer / mettre à jour l'UBO de post-processing et le lier au descriptor set de copy.
4. Valider visuellement et via CTest.

**Critères d'acceptation :**
- `m_copyLayout` contient 2 bindings (sampler + UBO).
- Un `UniformBuffer` de post-processing est créé, mis à jour et lié.
- `copy.frag` reçoit des valeurs cohérentes (exposure=1.0, gamma=2.2 par défaut).
- `ctest` au vert.

---

## 1.bis Revalidation Critique du Bug (Obligatoire - Double Check)
- **Bug ID / Signalement :** N1 (nouveau, hors catalogue initial — découvert lors de la revue du 2026-09-17)
- **Diagnostic critique indépendant :** Absence confirmée du binding 1 dans `m_copyLayout` et absence d'UBO lié. En mode offscreen, cela provoquait des erreurs `VUID-vkCmdDraw-None-08114` et des GPU faults entraînant le blocage des tests.
- **Preuve technique / Scénario de panne :** `assets/shaders/copy.frag:7-12` déclare `layout(set=0, binding=1) uniform PostProcessUBO { ... }`. `Renderer.cpp:334-336` crée `m_copyLayout` avec un seul `vk::DescriptorSetLayoutBinding` (binding 0, type `eCombinedImageSampler`). Le binding 1 n'existe pas dans le layout → le descriptor set alloué n'a pas d'UBO → `pp.*` lit des données indéfinies.
- **Statut Double Check :** [x] CONFIRMÉ (Bug réel et reproductible)
  - **Preuve runtime (log 2026-09-17) :** Validation Layer Khronos a émis : `vkCreateGraphicsPipelines(): SPIR-V uses descriptor [Set 0, Binding 1, variable "pp"] but was not declared in the VkPipelineLayoutCreateInfo::pSetLayouts[0].` → confirmation directe que le layout ne déclare pas le binding UBO que `copy.frag` attend.

---

## 2. Découpage en Tâches Atomiques (Approche TDD)

### Tâche 1 : Ajouter le binding PostProcessUBO au layout et au pipeline
- **Fichiers modifiés :** `src/bb3d/render/Renderer.cpp`, `include/bb3d/render/Renderer.hpp`, `include/bb3d/core/Config.hpp`
- **Test unitaire associé :** `tests/unit_test_26_picking.cpp`
- **Étape 1 (Test) :** Valider l'exécution en offscreen sans éditeur sans crash.
- **Étape 2 (Vérification échec) :** Confirmé via plantage du test avant correctif.
- **Étape 3 (Code minimal) :**
  - Ajout du binding 1 (`eUniformBuffer`, count 1, `eFragment`) au `m_copyLayout`.
  - Création de `m_postProcessUbo` avec struct `PostProcessUBO`.
  - Liaison de l'UBO et de l'image source dans `m_copyDescriptorSets`.
  - Mise à jour dans `compositeToSwapchain` et rafraîchissement au resize de `m_renderTarget`.
- **Étape 4 (Vérification succès) :** `ctest -R unit_test_26_picking` (PASS / GREEN en 3.66s).
- **Étape 5 (Commit) :** `fix(render): bind PostProcessUBO in copy pipeline to fix tonemapping/gamma (N1)`

---

## 3. Grille de Revue & Checkpoints

### 🛡️ Checkpoints de l'Implémenteur (Avant soumission en revue)
- [x] **Double Check Bug :** L'absence du binding UBO confirmée dans le code source live.
- [x] **TDD & Tests :** `unit_test_26_picking` validé + `ctest` au vert.
- [x] **Standards C++ (`cpp-pro`) :**
  - [x] Zéro allocation dynamique dans le *Hot Path* (UBO pré-alloué, `upload()` uniquement).
  - [x] `PostProcessUBO` aligné std140 (`alignas(16)`).
  - [x] Code, commentaires, logs en **anglais**.
- [x] **Standards Vulkan (`vulkan-cpp`) :**
  - [x] Aucune fuite de Descriptor Sets.
  - [x] Layout cohérent entre shader et C++.
- [x] **Qualité du Build :** Zéro warning compilateur.
- [x] **Commits :** Commit atomique (`fix:`).

---

## 4. Grille de Revue des Reviewers (Revue de Code Systématique & Approbation)
- [x] **Double Check Validé :** La section 1.bis est renseignée.
- [x] **Architecture & Opacité (`AGENTS.md`) :**
  - [x] `PostProcessUBO` ne fuit pas de types Vulkan dans le code client.
  - [x] Les valeurs par défaut (exposure=1.0, gamma=2.2) sont cohérentes.
- [x] **Sécurité & Robustesse :**
  - [x] Validation Layers sans erreur sur le binding UBO.
- [x] **Validation CTest :** Tous les tests au vert.
- [x] **Décision Reviewer :** [x] APPROVED | [ ] CHANGES REQUESTED
- [x] **Historique :** 1 entrée compacte consignée dans `tasks/HISTORY.md`.

---

## 5. Journal des Échanges & Retours de Revue
- *2026-09-17* - **@agent** : Création de la fiche à partir de la revue de code indépendante.
- *2026-09-17* - **@Antigravity** : Implémentation du binding PostProcessUBO, liaison dans `createCopyPipeline`, propagation dans `GraphicsConfig` et validation sous CTest. Décision : APPROVED.
