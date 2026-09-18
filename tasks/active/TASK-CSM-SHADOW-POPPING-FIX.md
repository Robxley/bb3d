# [TASK-CSM-SHADOW-POPPING-FIX] : Correction du Popping et Disparition des Ombres CSM

- **Statut :** READY FOR CODE REVIEW
- **Auteur / Implémenteur :** @Antigravity
- **Reviewer(s) :** @bb3d-reviewer (Mistral Vibe CLI), @dev
- **Branche Git :** `fix/csm-shadow-popping`
- **Date de création :** 2026-09-18
- **Document de Conception :** `tasks/active/2026-09-18-csm-shadow-popping-fix-design.md`

---

## 1. Contexte & Objectif

Les ombres directionnelles en cascade (CSM) disparaissent et réapparaissent brutalement lorsque la caméra bouge ou pivote dans la scène. L'objectif est de corriger le calcul mathématique du sous-frustum dans `ShadowCascade.cpp`, d'adapter le Frustum Culling dans `Renderer.cpp` pour ne pas évincer les projeteurs d'ombres hors champ, et de corriger l'indexation de cascade dans les shaders PBR et Toon.

---

## 1.bis Revalidation Critique du Bug (Double Check)

- **Bug ID / Signalement :** Signalement utilisateur : ombres qui disparaissent et réapparaissent selon la position de la caméra.
- **Diagnostic critique indépendant :**
  1. `ShadowCascade.cpp:45-57` : La boucle `for (int i = 0; i < 4; ++i)` lit `viewCorners[i + 4]`, ne prenant en compte que $x = 1$ (moitié droite de l'écran). La moitié gauche est totalement exclue, décentrant la sphère englobante de la cascade.
  2. `Renderer.cpp:983-990` : `prepareRenderData()` purge de `m_renderCommands` tout objet non visible dans le frustum caméra (`m_frustum`), empêchant les objets hors-champ de projeter leur ombre dans le champ.
  3. `assets/shaders/pbr.frag:59-65` & `assets/shaders/toon.frag:48-54` : La boucle d'indexation commence à `i = 1`, sautant systématiquement la cascade 0 pour les objets proches et retombant sur la cascade 0 pour les objets hors portée.
- **Preuve technique :** Code inspecté et vérifié aux lignes citées.
- **Statut Double Check :** [x] CONFIRMÉ (Bug réel et reproductible)

---

## 2. Découpage en Tâches Atomiques (Approche TDD)

### Tâche 1 : Test Unitaire TDD Géométrique (`tests/unit_test_shadows.cpp`)
- **Fichiers modifiés :** `tests/unit_test_shadows.cpp`
- **Étape 1 (Test) :** Écrire un test mathématique vérifiant que les 8 coins réels du sub-frustum (gauche, droite, haut, bas, near, far) projettent tous dans le cube $[-1, 1] \times [-1, 1] \times [0, 1]$ de la matrice de vue-projection lumière `lightVP`.
- **Étape 2 (Vérification échec) :** Valider l'échec sur le code actuel (les coins gauches sortent du champ).

### Tâche 2 : Correction Mathématique Exacte dans `ShadowCascade.cpp`
- **Fichiers modifiés :** `src/bb3d/render/ShadowCascade.cpp`
- **Étape 1 :** Unprojeter les 4 coins NDC à travers `invProj`, calculer les rayons de vue, dériver les 8 sommets réels (nearZ et farZ), centrer la sphère englobante et positionner la caméra de lumière avec marge arrière.
- **Étape 2 :** Valider la réussite du test `unit_test_shadows` (PASS / GREEN).

### Tâche 3 : Préservation des Projeteurs d'Ombres dans `Renderer.cpp`
- **Fichiers modifiés :** `src/bb3d/render/Renderer.cpp`
- **Étape 1 :** Modifier le prédicat de frustum culling dans `prepareRenderData()` pour conserver les objets avec `castShadows == true` situés à portée d'ombre (`distSq <= shadowFarZ * shadowFarZ`).
- **Étape 2 :** Vérifier que les objets hors-champ continuent de générer leurs commandes d'ombres dans `renderShadows()`.

### Tâche 4 : Correction de la Sélection de Cascade dans les Shaders
- **Fichiers modifiés :** `assets/shaders/pbr.frag`, `assets/shaders/toon.frag`
- **Étape 1 :** Corriger la boucle de cascade pour démarrer à `i = 0` et gérer le hors-cascade proprement (`return 1.0`).
- **Étape 2 :** Recompiler les shaders via CMake / `glslc` et valider l'absence de warning/erreur.

### Tâche 5 : Validation Globale & Suite CTest
- **Vérification :** Build complet `Debug` sans warning, suite CTest 100% verte.

---

## 3. Grille de Revue & Checkpoints

### 🛠️ Checkpoints de l'Implémenteur (Avant soumission en revue)
- [x] **Double Check Bug :** Les 3 causes racines confirmées dans le code live avant implémentation.
- [x] **TDD & Tests :** `unit_test_shadows` et l'ensemble de la suite de tests unitaires passent à 100%.
- [x] **Standards C++ (`cpp-pro`) :** Zéro allocation dynamique dans le hot-path, commentaires en anglais.
- [x] **Standards Vulkan (`vulkan-cpp`) :** `pipelineBarrier2` préservé, 0 warning Validation Layers.
- [x] **Qualité du Build :** Zéro warning compilateur MSVC.

---

### 🔍 Checkpoints des Reviewers (Revue de Code Systématique & Approbation)
- [ ] **Architecture & Stabilité :** Le calcul du frustum d'ombre est isotrope et symétrique, plus de popping au bord d'écran.
- [ ] **Performance :** L'instancing d'ombres et le culling ne causent pas de régression de framerate.
- [ ] **Décision Reviewer :** [ ] APPROVED | [ ] CHANGES REQUESTED

---

## 4. Journal des Échanges & Retours de Revue
- *2026-09-18* - **@Antigravity** : Création du document de conception et de la fiche de tâche après double-check critique du bug.
- *2026-09-18* - **@bb3d-reviewer (glm-5.2)** : Revue d'architecture validant les 3 causes racines et la stratégie TDD.
- *2026-09-18* - **@Antigravity** : Implémentation TDD complète (RED -> GREEN), refonte mathématique fermée 8 coins dans `ShadowCascade.cpp`, préservation des casters dans `Renderer.cpp`, boucle de cascades sécurisée dans `pbr.frag` et `toon.frag`, suite CTest 100% PASS sans warning.
