# [TASK-TEST-INFRA] : Harnais de test cassé + CMake + Mise à jour AGENTS.md (N12, CMake, R1)

- **Statut :** DRAFT
- **Auteur / Implémenteur :** @bb3d-fixer
- **Reviewer(s) :** @bb3d-reviewer, @dev
- **Branche Git :** `fix/test-infra-agents-md`
- **Date de création :** 2026-09-17

---

## 1. Contexte & Objectif

Trois problèmes d'infrastructure de test :
- **N12 (HIGH)** : `unit_test_13_camera.cpp:9` — la macro `BB_ASSERT_TRUE` fait `return;` dans une fonction de test `void` → l'assertion n'échoue jamais, `main()` retourne toujours 0. **Le seul test de logique caméra est non fonctionnel.**
- **CMake** : `CMakeLists.txt:449` — `file(GLOB TEST_SOURCES "tests/unit_test_*.cpp")` sans `CONFIGURE_DEPENDS` (contrairement aux sources moteur ligne 309). Les nouveaux tests ne sont pas détectés sans reconfigure manuelle.
- **AGENTS.md R1** : La règle d'opacité API interdit `vk::*` dans le code client, mais les tests/unitaires peuvent avoir besoin d'accéder aux types Vulkan pour valider le rendu (ex: `unit_test_shadows.cpp:30` utilise `vk::ImageView`). Il faut exempter explicitement les tests de cette règle.

**Objectif :**
1. Réparer le harnais de test pour que les assertions échouent réellement (propagation du code de retour).
2. Ajouter `CONFIGURE_DEPENDS` au GLOB des tests.
3. Mettre à jour `AGENTS.md` pour exempter les tests de la règle d'opacité API.
4. Valider CTest.

**Critères d'acceptation :**
- `unit_test_13_camera` échoue réellement si une assertion est violée (retourne non-zéro).
- Les nouveaux fichiers de test sont détectés automatiquement par CMake.
- `AGENTS.md` exempt explicitement `tests/` de la règle R1 (opacité API).
- `ctest` au vert.

---

## 1.bis Revalidation Critique du Bug (Obligatoire - Double Check)

### N12 — Harnais de test cassé
- **Bug ID / Signalement :** N12 (nouveau, découvert lors de la revue du 2026-09-17)
- **Diagnostic critique indépendant :** [À renseigner par @bb3d-fixer]
- **Preuve technique / Scénario de panne :** `tests/unit_test_13_camera.cpp:9` — `BB_ASSERT_TRUE` macro fait `return;` sur échec dans une fonction `void`. Le `main()` retourne toujours 0. Les assertions caméra ne peuvent jamais faire échouer le test.
- **Statut Double Check :** [ ] CONFIRMÉ / [ ] RÉFUTÉ

### CMake — GLOB tests sans CONFIGURE_DEPENDS
- **Bug ID / Signalement :** Découvert lors de la revue du 2026-09-17
- **Diagnostic critique indépendant :** [À renseigner par @bb3d-fixer]
- **Preuve technique :** `CMakeLists.txt:449` — `file(GLOB TEST_SOURCES "tests/unit_test_*.cpp")` sans `CONFIGURE_DEPENDS`. Comparer avec `CMakeLists.txt:309` qui utilise `CONFIGURE_DEPENDS` pour les sources moteur.
- **Statut Double Check :** [ ] CONFIRMÉ / [ ] RÉFUTÉ

### AGENTS.md R1 — Exemption des tests
- **Bug ID / Signalement :** Décision utilisateur du 2026-09-17
- **Diagnostic :** La règle R1 ("Le code client ne doit JAMAIS inclure de headers Vulkan") est trop stricte pour les tests qui doivent valider le rendu. `unit_test_shadows.cpp:30` utilise déjà `vk::ImageView`.
- **Action :** Ajouter une exemption explicite pour `tests/` dans `AGENTS.md`.

---

## 2. Découpage en Tâches Atomiques (Approche TDD)

### Tâche 1 : Réparer le harnais de test unit_test_13 (N12)
- **Fichiers modifiés :** `tests/unit_test_13_camera.cpp` (et potentiellement le fichier de macro `BB_ASSERT_TRUE` si partagé)
- **Test unitaire associé :** Le test lui-même — vérifier qu'une assertion violée retourne un code non-zéro.
- **Étape 1 (Test) :** Écrire un cas d'assertion délibérément faux et vérifier que `ctest` le marque FAILED.
- **Étape 2 (Vérification échec) :** `cmake --build build --config Debug -j && ctest --test-dir build -C Debug -R camera --output-on-failure` (RED).
- **Étape 3 (Code minimal) :** Modifier `BB_ASSERT_TRUE` pour qu'il propage l'échec au `main()` (ex: variable globale/atomic `g_testFailed`, ou `return -1` depuis `main()` si l'assertion échoue). Vérifier si la macro est partagée par d'autres tests et éviter les régressions.
- **Étape 4 (Vérification succès) :** `ctest` — le test faux échoue, le test normal passe (GREEN).
- **Étape 5 (Commit) :** `fix(test): repair BB_ASSERT_TRUE to propagate failures in unit_test_13_camera (N12)`

### Tâche 2 : Ajouter CONFIGURE_DEPENDS au GLOB des tests
- **Fichiers modifiés :** `CMakeLists.txt`
- **Étape 3 (Code minimal) :** Changer `file(GLOB TEST_SOURCES "tests/unit_test_*.cpp")` (ligne 449) en `file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "tests/unit_test_*.cpp")`.
- **Étape 4 (Vérification) :** Ajouter un fichier de test temporaire, reconfigurer, vérifier qu'il est détecté, puis le supprimer.
- **Étape 5 (Commit) :** `fix(cmake): add CONFIGURE_DEPENDS to test sources glob`

### Tâche 3 : Mettre à jour AGENTS.md — exemption tests pour R1
- **Fichiers modifiés :** `AGENTS.md`
- **Étape 3 (Code minimal) :** Dans la section "Règles de Codage & Standards Critiques", règle 1 (Opacité de l'API Publique), ajouter une exemption explicite : "Les tests unitaires (`tests/`) sont exemptés de cette règle : l'utilisation de types `vk::*` est autorisée dans les tests pour faciliter la validation du rendu."
- **Étape 5 (Commit) :** `docs(agents): exempt tests from API opacity rule (R1) to enable render validation`

---

## 3. Grille de Revue & Checkpoints

### 🛡️ Checkpoints de l'Implémenteur (Avant soumission en revue)
- [ ] **Double Check Bug :** N12 et le problème CMake confirmés dans le code live.
- [ ] **TDD & Tests :**
  - [ ] `unit_test_13_camera` échoue réellement si une assertion est violée.
  - [ ] Les autres tests utilisant `BB_ASSERT_TRUE` (si partagé) ne sont pas cassés.
  - [ ] `ctest` complet au vert.
- [ ] **Standards :**
  - [ ] `AGENTS.md` mis à jour avec l'exemption tests (en français, cohérent avec le reste du document).
- [ ] **Qualité du Build :** Zéro warning compilateur.
- [ ] **Commits :** 3 commits atomiques (`fix:` / `docs:`).

---

### 🔍 Checkpoints des Reviewers (Revue de Code Systématique & Approbation)
- [ ] **Double Check Validé :** La section 1.bis est renseignée.
- [ ] **Sécurité & Robustesse :**
  - [ ] La macro `BB_ASSERT_TRUE` corrigée ne casse pas les autres tests qui l'utilisent.
  - [ ] L'exemption AGENTS.md est clairement limitée à `tests/` (pas d'apps/ ni de src/ client).
- [ ] **Validation CTest :** Tous les tests au vert, les assertions échouent correctement.
- [ ] **Décision Reviewer :** [ ] APPROVED | [ ] CHANGES REQUESTED
- [ ] **Historique :** 1 entrée compacte consignée dans `tasks/HISTORY.md`.

---

## 4. Journal des Échanges & Retours de Revue
- *2026-09-17* - **@agent** : Création de la fiche à partir de la revue de code indépendante du 2026-09-17.
- *2026-09-17* - **@user** : Validation de l'exemption des tests pour la règle d'opacité API (R1).
