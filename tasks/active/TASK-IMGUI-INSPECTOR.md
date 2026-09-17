# [TASK-IMGUI-INSPECTOR] : Inspecteur Light mort + états partagés (N5, N6, N7)

- **Statut :** DRAFT
- **Auteur / Implémenteur :** @bb3d-fixer
- **Reviewer(s) :** @bb3d-reviewer, @dev
- **Branche Git :** `fix/imgui-inspector-light-static`
- **Date de création :** 2026-09-17

---

## 1. Contexte & Objectif

Trois bugs dans `ImGuiLayer.cpp` affectent l'éditeur :
- **N5 (HIGH)** : Un `else if` dupliqué à l'identique rend l'inspecteur de composant Light **inaccessible** (branche vide gagne toujours).
- **N6 (MEDIUM)** : `static glm::vec4 partCol` dans l'inspecteur ParticleSystem partage la couleur entre toutes les instances.
- **N7 (MEDIUM)** : `static ModelLoadConfig s_loadConfig` partagé entre toutes les entités inspectées.

**Objectif :**
1. Confirmer les 3 bugs dans le code live (Double Check).
2. Supprimer le `else if` dupliqué pour rétablir l'inspecteur Light.
3. Remplacer les `static` locaux par un état par-entité ou par-composant.
4. Valider via CTest.

**Critères d'acceptation :**
- L'inspecteur Light s'affiche correctement dans l'éditeur.
- La couleur ParticleSystem est propre à chaque instance.
- Le `ModelLoadConfig` ne contamine pas les autres entités.
- `ctest` au vert.

---

## 1.bis Revalidation Critique du Bug (Obligatoire - Double Check)

### N5 — Inspecteur Light mort
- **Bug ID / Signalement :** N5 (nouveau, découvert lors de la revue du 2026-09-17)
- **Diagnostic critique indépendant :** [À renseigner par @bb3d-fixer]
- **Preuve technique / Scénario de panne :** `ImGuiLayer.cpp:823` = `else if (m_focusedComponent == "Light" && m_selectedEntity.has<LightComponent>()) {}` (branche vide). `ImGuiLayer.cpp:824` = `else if` identique avec le corps de l'inspecteur. La branche 823 gagne toujours → l'inspecteur Light (lignes 825-834) est du code mort.
- **Statut Double Check :** [ ] CONFIRMÉ / [ ] RÉFUTÉ

### N6 — Couleur ParticleSystem partagée
- **Bug ID / Signalement :** N6
- **Diagnostic critique indépendant :** [À renseigner par @bb3d-fixer]
- **Preuve technique / Scénario de panne :** `ImGuiLayer.cpp:1035` — `static glm::vec4 partCol` persiste entre les appels. Éditer la couleur de l'entité A, puis inspecter l'entité B → B affiche la couleur de A.
- **Statut Double Check :** [ ] CONFIRMÉ / [ ] RÉFUTÉ

### N7 — ModelLoadConfig partagé
- **Bug ID / Signalement :** N7
- **Diagnostic critique indépendant :** [À renseigner par @bb3d-fixer]
- **Preuve technique / Scénario de panne :** `ImGuiLayer.cpp:776` — `static ModelLoadConfig s_loadConfig` partagé entre toutes les entités. Le preset de chargement d'une entité contamine les autres.
- **Statut Double Check :** [ ] CONFIRMÉ / [ ] RÉFUTÉ

---

## 2. Découpage en Tâches Atomiques (Approche TDD)

### Tâche 1 : Supprimer le else if Light dupliqué (N5)
- **Fichiers modifiés :** `src/bb3d/core/ImGuiLayer.cpp`
- **Test unitaire associé :** Pas de test automatisé direct (UI ImGui). Validation manuelle dans l'éditeur + vérification que le code compile et que les autres inspecteurs fonctionnent.
- **Étape 1 (Test) :** Ajouter un test de smoke vérifiant que `ImGuiLayer` s'initialise et rend sans crash (test existant si présent).
- **Étape 2 (Vérification échec) :** N/A (bug UI, pas de test automatisé direct).
- **Étape 3 (Code minimal) :** Supprimer la ligne 823 (branche vide dupliquée). Conserver la ligne 824 avec le corps de l'inspecteur Light.
- **Étape 4 (Vérification succès) :** `cmake --build build --config Debug -j && ctest --test-dir build -C Debug --output-on-failure` (compile + tests existants).
- **Étape 5 (Commit) :** `fix(editor): remove duplicate else-if to restore Light component inspector (N5)`

### Tâche 2 : État par-entité pour ParticleSystem tint (N6)
- **Fichiers modifiés :** `src/bb3d/core/ImGuiLayer.cpp`
- **Étape 1 (Test) :** Smoke test de compilation + tests existants.
- **Étape 3 (Code minimal) :** Remplacer `static glm::vec4 partCol` par une lecture depuis le matériau de la particule courante, ou un `std::unordered_map<entt::entity, glm::vec4>` membre de `ImGuiLayer`.
- **Étape 5 (Commit) :** `fix(editor): per-entity particle tint state instead of shared static (N6)`

### Tâche 3 : État par-entité pour ModelLoadConfig (N7)
- **Fichiers modifiés :** `src/bb3d/core/ImGuiLayer.cpp`
- **Étape 3 (Code minimal) :** Remplacer `static ModelLoadConfig s_loadConfig` par un membre de `ImGuiLayer` mappé par entité, ou stocker le config dans le `ModelComponent` lui-même.
- **Étape 5 (Commit) :** `fix(editor): per-entity ModelLoadConfig instead of shared static (N7)`

---

## 3. Grille de Revue & Checkpoints

### 🛡️ Checkpoints de l'Implémenteur (Avant soumission en revue)
- [ ] **Double Check Bug :** Les 3 bugs (N5, N6, N7) confirmés dans le code source live.
- [ ] **TDD & Tests :** Tests existants passent (`ctest`).
- [ ] **Standards C++ (`cpp-pro`) :**
  - [ ] Zéro `static` local partagé entre entités dans l'inspecteur.
  - [ ] Code, commentaires, logs en **anglais**.
- [ ] **Qualité du Build :** Zéro warning compilateur.
- [ ] **Commits :** 3 commits atomiques (`fix:`).

---

### 🔍 Checkpoints des Reviewers (Revue de Code Systématique & Approbation)
- [ ] **Double Check Validé :** La section 1.bis est renseignée pour les 3 bugs.
- [ ] **Architecture & Opacité (`AGENTS.md`) :**
  - [ ] Pas de régression sur les autres inspecteurs (Mesh, Material, Camera, etc.).
- [ ] **Sécurité & Robustesse :**
  - [ ] Les maps par-entité sont nettoyées à la destruction d'entité.
- [ ] **Validation CTest :** Tous les tests au vert.
- [ ] **Décision Reviewer :** [ ] APPROVED | [ ] CHANGES REQUESTED
- [ ] **Historique :** 1 entrée compacte consignée dans `tasks/HISTORY.md`.

---

## 4. Journal des Échanges & Retours de Revue
- *2026-09-17* - **@agent** : Création de la fiche à partir de la revue de code indépendante du 2026-09-17.
