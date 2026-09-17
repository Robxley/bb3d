# [TASK-ASTRO-FIXES] : Corrections AstroBazard — HUD, typo shadow, input, config (N10, N11, B27, N13)

- **Statut :** DRAFT
- **Auteur / Implémenteur :** @bb3d-fixer
- **Reviewer(s) :** @bb3d-reviewer, @dev
- **Branche Git :** `fix/astrobazard-hud-input`
- **Date de création :** 2026-09-17

---

## 1. Contexte & Objectif

Trois bugs dans l'application `apps/astro_bazard/main.cpp` :
- **N10 (MEDIUM)** : Le réticule HUD spatial est verticalement retourné. `screenY = (ndc.y + 1.0f) * 0.5f * winH` mappe NDC-top (y=+1) vers le bas de l'écran (winH), mais ImGui Y croît vers le bas.
- **N11 (LOW)** : `setShadows(true, 2024, 4, true)` — 2024 n'est pas une puissance de 2. Typo probable pour 2048.
- **B27 (MEDIUM)** : `Key::R` (import) et `Key::S` (save) utilisent `isKeyPressed` (maintenu) au lieu de `isKeyJustPressed`. Maintenir R/S déclenche l'import/export à chaque frame.
- **N13 (MEDIUM, nouveau)** : `config/engine_config.json` ne contient pas les clés `enableEditor` et `pickingMode` dans la section `modules`, mais elles sont déclarées dans `NLOHMANN_DEFINE_TYPE_INTRUSIVE(ModuleConfig, ...)` (`Config.hpp:133`). La macro lève `nlohmann::json::out_of_range` à la désérialisation → toute la section `modules` du fichier est ignorée (fallback silencieux sur les defaults C++).

**Objectif :**
1. Corriger l'inversion Y du HUD.
2. Corriger la typo 2024 → 2048.
3. Utiliser `isKeyJustPressed` pour R et S.
4. Ajouter les clés manquantes au JSON de config (ou migrer vers `NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT`).
5. Valider via CTest.

**Critères d'acceptation :**
- Le réticule HUD s'affiche à la bonne position verticale.
- La shadow map utilise une résolution puissance de 2 (2048).
- R et S ne déclenchent l'import/export qu'une seule fois par pression.
- Aucune exception `out_of_range` au chargement de `engine_config.json`.
- `ctest` au vert.

---

## 1.bis Revalidation Critique du Bug (Obligatoire - Double Check)

### N10 — HUD reticle Y inversé
- **Bug ID / Signalement :** N10 (nouveau, découvert lors de la revue du 2026-09-17)
- **Diagnostic critique indépendant :** [À renseigner par @bb3d-fixer]
- **Preuve technique / Scénario de panne :** `apps/astro_bazard/main.cpp:257-260` — `float screenY = (ndc.y + 1.0f) * 0.5f * winH;`. NDC y=+1 = haut de l'écran, mais `screenY = winH` = bas en coordonnées ImGui (Y croît vers le bas). Le réticule est verticalement retourné.
- **Statut Double Check :** [ ] CONFIRMÉ / [ ] RÉFUTÉ

### N11 — Typo résolution shadow map
- **Bug ID / Signalement :** N11 (nouveau)
- **Diagnostic critique indépendant :** [À renseigner par @bb3d-fixer]
- **Preuve technique :** `apps/astro_bazard/main.cpp:479` — `config.graphics.setShadows(true, 2024, 4, true);`. 2024 n'est pas une puissance de 2 → shadow map non-PoT (sous-optimal, warning validation).
- **Statut Double Check :** [ ] CONFIRMÉ / [ ] RÉFUTÉ

### B27 — isKeyPressed au lieu de isKeyJustPressed pour R/S
- **Bug ID / Signalement :** B27 dans `tasks/CODE_REVIEW.md`
- **Diagnostic critique indépendant :** [À renseigner par @bb3d-fixer]
- **Preuve technique :** `apps/astro_bazard/main.cpp:77-80` (R/import) et `:141-143` (S/save) utilisent `isKeyPressed`. Backspace (`:144`) utilise correctement `isKeyJustPressed`. Maintenir R ou S déclenche l'opération à chaque frame.
- **Statut Double Check :** [x] CONFIRMÉ (Bug réel et reproductible)
  - **Preuve runtime (log 2026-09-17) :** 3 appels à `SceneSerializer: Serialized scene to astro_bazard_scene.json` en 1 seconde (16:44:45-46) pendant que l'utilisateur maintient la touche S.

### N13 — Clés manquantes dans engine_config.json
- **Bug ID / Signalement :** N13 (nouveau, découvert via log runtime le 2026-09-17)
- **Diagnostic critique indépendant :** [À renseigner par @bb3d-fixer]
- **Preuve technique :** `config/engine_config.json:37-43` — section `modules` contient `enablePhysics`, `physicsBackend`, `enableAudio`, `enableJobSystem`, `enableHotReload` mais PAS `enableEditor` ni `pickingMode`. `Config.hpp:133` déclare `NLOHMANN_DEFINE_TYPE_INTRUSIVE(ModuleConfig, physicsBackend, enablePhysics, enableAudio, enableHotReload, enableEditor, pickingMode)` — la macro exige toutes les clés listées → `out_of_range` à la désérialisation.
- **Statut Double Check :** [x] CONFIRMÉ (Bug réel et reproductible)
  - **Preuve runtime (log 2026-09-17) :** `[json.exception.out_of_range.403] key 'enableEditor' not found — Using defaults.`

---

## 2. Découpage en Tâches Atomiques (Approche TDD)

### Tâche 1 : Corriger l'inversion Y du HUD (N10)
- **Fichiers modifiés :** `apps/astro_bazard/main.cpp`
- **Étape 3 (Code minimal) :** Remplacer `float screenY = (ndc.y + 1.0f) * 0.5f * winH;` par `float screenY = (1.0f - (ndc.y + 1.0f) * 0.5f) * winH;` (ligne 259).
- **Étape 4 (Vérification) :** `cmake --build build --config Debug -j && ctest --test-dir build -C Debug --output-on-failure`.
- **Étape 5 (Commit) :** `fix(astro): correct HUD reticle Y-axis flip (N10)`

### Tâche 2 : Corriger la typo résolution shadow map (N11)
- **Fichiers modifiés :** `apps/astro_bazard/main.cpp`
- **Étape 3 (Code minimal) :** Remplacer `2024` par `2048` (ligne 479).
- **Étape 5 (Commit) :** `fix(astro): correct shadow map resolution 2024 to 2048 (N11)`

### Tâche 3 : isKeyJustPressed pour R et S (B27)
- **Fichiers modifiés :** `apps/astro_bazard/main.cpp`
- **Étape 3 (Code minimal) :** Remplacer `input.isKeyPressed(Key::R)` par `input.isKeyJustPressed(Key::R)` (ligne 77) et `input.isKeyPressed(Key::S)` par `input.isKeyJustPressed(Key::S)` (ligne 141).
- **Étape 5 (Commit) :** `fix(astro): use isKeyJustPressed for R/S to prevent per-frame import/export spam (B27)`

### Tâche 4 : Corriger les clés manquantes dans engine_config.json (N13)
- **Fichiers modifiés :** `config/engine_config.json` (et potentiellement `include/bb3d/core/Config.hpp`)
- **Étape 3 (Code minimal) :** Deux options (choisir la plus robuste) :
  - Option A : Ajouter `"enableEditor": true` et `"pickingMode": "ColorPicking"` au JSON dans la section `modules`.
  - Option B (préférable) : Migrer `NLOHMANN_DEFINE_TYPE_INTRUSIVE` vers `NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT` pour `ModuleConfig` (et les autres configs) afin que les clés manquantes utilisent les defaults C++ sans lever d'exception.
- **Étape 4 (Vérification) :** Lancer `astro_bazard.exe` et vérifier l'absence de l'exception `[json.exception.out_of_range.403]` dans les logs.
- **Étape 5 (Commit) :** `fix(config): add missing enableEditor/pickingMode keys to engine_config.json (N13)`

---

## 3. Grille de Revue & Checkpoints

### 🛡️ Checkpoints de l'Implémenteur (Avant soumission en revue)
- [ ] **Double Check Bug :** Les 3 bugs confirmés dans le code source live.
- [ ] **TDD & Tests :** `ctest` au vert.
- [ ] **Standards C++ (`cpp-pro`) :**
  - [ ] Code, commentaires, logs en **anglais** (corriger les logs français si touchés).
- [ ] **Qualité du Build :** Zéro warning compilateur.
- [ ] **Commits :** 3 commits atomiques (`fix:`).

---

### 🔍 Checkpoints des Reviewers (Revue de Code Systématique & Approbation)
- [ ] **Double Check Validé :** La section 1.bis est renseignée.
- [ ] **Sécurité & Robustesse :**
  - [ ] Le réticule s'affiche à la bonne position pour un objet en haut/bas de l'écran.
  - [ ] R/S ne se déclenchent qu'une fois par pression physique.
- [ ] **Validation CTest :** Tous les tests au vert.
- [ ] **Décision Reviewer :** [ ] APPROVED | [ ] CHANGES REQUESTED
- [ ] **Historique :** 1 entrée compacte consignée dans `tasks/HISTORY.md`.

---

## 4. Journal des Échanges & Retours de Revue
- *2026-09-17* - **@agent** : Création de la fiche à partir de la revue de code indépendante du 2026-09-17.
