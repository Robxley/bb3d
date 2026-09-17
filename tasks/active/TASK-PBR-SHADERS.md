# [TASK-PBR-SHADERS] : Corrections de shaders PBR/Toon (N2, N3, N4, ND3)

- **Statut :** DRAFT
- **Auteur / Implémenteur :** @bb3d-fixer
- **Reviewer(s) :** @bb3d-reviewer, @dev
- **Branche Git :** `fix/pbr-toon-shaders`
- **Date de création :** 2026-09-17

---

## 1. Contexte & Objectif

Quatre problèmes dans les shaders GLSL :
- **N2 (MEDIUM)** : La formule de roughness dans `pbr.frag` est non standard (`mix(0.5, orm.g, factor)` au lieu de `orm.g * factor`). Un matériau avec `roughnessFactor=0` ne devient jamais un miroir.
- **N3 (LOW)** : Sélection de cascade d'ombres incohérente entre `toon.frag` (`depth < splitDepths[i]`) et `pbr.frag` (`depth <= splitDepths[i]`).
- **N4 (LOW)** : `toon.frag` hardcode les biais d'ombre (`0.005`/`0.001`) au lieu d'utiliser `ubo.shadowBiases` comme `pbr.frag`.
- **ND3** : Varyings `fragPos`/`fragNormal` déclarés mais jamais lus dans `unlit.frag` et `particle.frag` (bande passante gaspillée).

**Objectif :**
1. Corriger la formule roughness PBR.
2. Harmoniser la sélection de cascade et les biais d'ombre entre toon et pbr.
3. Supprimer les varyings morts.
4. Recompiler les shaders SPIR-V et valider visuellement + CTest.

**Critères d'acceptation :**
- `roughness = orm.g * mat.roughnessFactor` dans `pbr.frag`.
- `toon.frag` utilise `depth <=` et `ubo.shadowBiases`.
- `unlit.frag` et `particle.frag` ne déclarent plus `fragPos`/`fragNormal`.
- Shaders recompilés en `.spv` et `ctest` au vert.

---

## 1.bis Revalidation Critique du Bug (Obligatoire - Double Check)

### N2 — Formule roughness PBR non standard
- **Bug ID / Signalement :** N2 (nouveau, découvert lors de la revue du 2026-09-17)
- **Diagnostic critique indépendant :** [À renseigner par @bb3d-fixer]
- **Preuve technique / Scénario de panne :** `assets/shaders/pbr.frag:143` — `float roughness = mix(0.5, orm.g, mat.roughnessFactor);`. Quand `roughnessFactor=0` → `roughness=0.5` (devrait être 0 = miroir parfait). Un commentaire de dev est laissé dans le shader ("Wait, roughness is usually multiplied...").
- **Statut Double Check :** [ ] CONFIRMÉ / [ ] RÉFUTÉ

### N3 — Sélection de cascade incohérente
- **Bug ID / Signalement :** N3
- **Diagnostic critique indépendant :** [À renseigner par @bb3d-fixer]
- **Preuve technique / Scénario de panne :** `assets/shaders/toon.frag:48-55` utilise `depth < splitDepths[i]` (strict). `assets/shaders/pbr.frag:60-65` utilise `depth <= splitDepths[i]`. Frontières de cascade différentes → inconsistances visuelles entre shaders.
- **Statut Double Check :** [ ] CONFIRMÉ / [ ] RÉFUTÉ

### N4 — Biais d'ombre hardcoded dans toon
- **Bug ID / Signalement :** N4
- **Diagnostic critique indépendant :** [À renseigner par @bb3d-fixer]
- **Preuve technique / Scénario de panne :** `assets/shaders/toon.frag:65` utilise `0.005`/`0.001` au lieu de `ubo.shadowBiases`.
- **Statut Double Check :** [ ] CONFIRMÉ / [ ] RÉFUTÉ

### ND3 — Varyings morts
- **Bug ID / Signalement :** ND3
- **Diagnostic critique indépendant :** [À renseigner par @bb3d-fixer]
- **Preuve technique :** `unlit.frag:3-4` et `particle.frag:3-4` déclarent `fragPos`/`fragNormal` jamais lus.
- **Statut Double Check :** [ ] CONFIRMÉ / [ ] RÉFUTÉ

---

## 2. Découpage en Tâches Atomiques (Approche TDD)

### Tâche 1 : Corriger la formule roughness PBR (N2)
- **Fichiers modifiés :** `assets/shaders/pbr.frag`
- **Test unitaire associé :** `tests/unit_test_16_materials.cpp` (existant) — valider visuellement que roughness=0 donne un miroir.
- **Étape 1 (Test) :** Étendre `unit_test_16` si possible pour vérifier la sortie visuelle avec roughnessFactor=0.
- **Étape 3 (Code minimal) :** Remplacer `mix(0.5, orm.g, mat.roughnessFactor)` par `orm.g * mat.roughnessFactor`. Supprimer le commentaire de dev. Ajouter un clamp `[0.0, 1.0]` pour sécurité.
- **Étape 4 (Vérification) :** Recompiler le shader `.spv`, `ctest` (PASS).
- **Étape 5 (Commit) :** `fix(shader): correct PBR roughness formula to standard multiply (N2)`

### Tâche 2 : Harmoniser sélection de cascade toon/pbr (N3)
- **Fichiers modifiés :** `assets/shaders/toon.frag`
- **Étape 3 (Code minimal) :** Changer `depth < splitDepths[i]` en `depth <= splitDepths[i]` dans `toon.frag`.
- **Étape 5 (Commit) :** `fix(shader): unify cascade selection comparison between toon and pbr (N3)`

### Tâche 3 : Utiliser ubo.shadowBiases dans toon (N4)
- **Fichiers modifiés :** `assets/shaders/toon.frag`
- **Étape 3 (Code minimal) :** Remplacer `0.005`/`0.001` par `ubo.shadowBiases.x`/`ubo.shadowBiases.y` (vérifier les noms exacts dans la définition UBO de `toon.frag`).
- **Étape 5 (Commit) :** `fix(shader): use ubo shadowBiases in toon shader instead of hardcoded values (N4)`

### Tâche 4 : Supprimer les varyings morts (ND3)
- **Fichiers modifiés :** `assets/shaders/unlit.vert`, `assets/shaders/unlit.frag`, `assets/shaders/particle.vert`, `assets/shaders/particle.frag`
- **Étape 3 (Code minimal) :** Retirer `fragPos` et `fragNormal` des déclarations `out`/`in` dans les 4 fichiers.
- **Étape 5 (Commit) :** `refactor(shader): remove unused fragPos/fragNormal varyings from unlit and particle (ND3)`

---

## 3. Grille de Revue & Checkpoints

### 🛡️ Checkpoints de l'Implémenteur (Avant soumission en revue)
- [ ] **Double Check Bug :** Les 4 bugs confirmés dans les shaders live.
- [ ] **TDD & Tests :** Shaders recompilés en `.spv`, `ctest` au vert.
- [ ] **Standards :**
  - [ ] Shaders en anglais, commentaires de dev supprimés.
  - [ ] Pas de varyings inutilisés.
- [ ] **Qualité du Build :** Zéro warning compilateur de shader.
- [ ] **Commits :** 4 commits atomiques (`fix:` / `refactor:`).

---

### 🔍 Checkpoints des Reviewers (Revue de Code Systématique & Approbation)
- [ ] **Double Check Validé :** La section 1.bis est renseignée.
- [ ] **Cohérence visuelle :**
  - [ ] Roughness=0 produit un miroir, roughness=1 produit un diffuse pur.
  - [ ] Pas de régression de seam entre cascades.
  - [ ] Toon et PBR cohérents sur les bords de cascade.
- [ ] **Validation CTest :** Tous les tests au vert.
- [ ] **Décision Reviewer :** [ ] APPROVED | [ ] CHANGES REQUESTED
- [ ] **Historique :** 1 entrée compacte consignée dans `tasks/HISTORY.md`.

---

## 4. Journal des Échanges & Retours de Revue
- *2026-09-17* - **@agent** : Création de la fiche à partir de la revue de code indépendante du 2026-09-17.
