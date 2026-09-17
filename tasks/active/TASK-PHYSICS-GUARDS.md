# [TASK-PHYSICS-GUARDS] : Robustesse physique Jolt (B14, B15, B20, B16, B21)

- **Statut :** DRAFT
- **Auteur / Implémenteur :** @bb3d-fixer
- **Reviewer(s) :** @bb3d-reviewer, @dev
- **Branche Git :** `fix/physics-guards-b14-b21`
- **Date de création :** 2026-09-17

---

## 1. Contexte & Objectif

Cinq bugs de robustesse dans le système physique Jolt :
- **B14 (MEDIUM)** : `PhysicsWorld.cpp:127` — `(int)std::thread::hardware_concurrency() - 1` non clamé. Si `hardware_concurrency()` retourne 0 ou 1 → `-1` ou `0` passé à `JPH::JobSystemThreadPool` → UB/crash.
- **B15 (MEDIUM)** : `PhysicsWorld.cpp:294-298` — `createRigidBody` appelle `entity.get<TransformComponent>()` sans garde `has<>()`. Une entité physique sans transform lève/aborte.
- **B20 (MEDIUM)** : `PhysicsWorld.cpp:397` — `createCharacterController` même problème que B15 (pas de garde `has<>()`).
- **B16 (MEDIUM)** : `PhysicsWorld.cpp:375-376` — `CreateBody` peut retourner `nullptr` (cap 1024 corps atteinte) ; `body->GetID()` déréférence null → crash.
- **B21 (MEDIUM)** : `Scene.cpp:154-166` — `destroyEntity` détruit l'entité sans appeler `physics().destroyRigidBody()` → corps Jolt orphelin/fuite.

**Objectif :**
1. Revalider les 5 bugs dans le code live (Double Check).
2. Ajouter les gardes et checks nécessaires.
3. Appeler `destroyRigidBody` dans `destroyEntity`.
4. Valider via CTest.

**Critères d'acceptation :**
- `hardware_concurrency` clamé à `std::max(1, ...)`.
- Gardes `has<TransformComponent>()` dans `createRigidBody` et `createCharacterController`.
- Check `body != nullptr` après `CreateBody`.
- `destroyEntity` nettoie le corps Jolt.
- `ctest` au vert.

---

## 1.bis Revalidation Critique du Bug (Obligatoire - Double Check)

### B14 — Thread count non clamé
- **Bug ID / Signalement :** B14 dans `tasks/CODE_REVIEW.md`
- **Diagnostic critique indépendant :** [À renseigner par @bb3d-fixer]
- **Preuve technique :** `PhysicsWorld.cpp:127` — `(int)std::thread::hardware_concurrency() - 1`. Si `hardware_concurrency()` retourne 0 (indéterminé) → `-1`. Comparer avec `JobSystem.cpp:16` qui clampe correctement avec `std::max(1u, ...)`.
- **Statut Double Check :** [ ] CONFIRMÉ / [ ] RÉFUTÉ

### B15 — createRigidBody sans garde Transform
- **Bug ID / Signalement :** B15 dans `tasks/CODE_REVIEW.md`
- **Diagnostic critique indépendant :** [À renseigner par @bb3d-fixer]
- **Preuve technique :** `PhysicsWorld.cpp:294-298` — `entity.get<TransformComponent>()` sans `has<>()`. Appelé depuis `update()` (195-201) sur toutes les entités à `PhysicsComponent`.
- **Statut Double Check :** [ ] CONFIRMÉ / [ ] RÉFUTÉ

### B20 — createCharacterController sans garde Transform
- **Bug ID / Signalement :** B20 dans `tasks/CODE_REVIEW.md`
- **Diagnostic critique indépendant :** [À renseigner par @bb3d-fixer]
- **Preuve technique :** `PhysicsWorld.cpp:397` — même problème que B15.
- **Statut Double Check :** [ ] CONFIRMÉ / [ ] RÉFUTÉ

### B16 — CreateBody nullptr non vérifié
- **Bug ID / Signalement :** B16 dans `tasks/CODE_REVIEW.md`
- **Diagnostic critique indépendant :** [À renseigner par @bb3d-fixer]
- **Preuve technique :** `PhysicsWorld.cpp:375-376` — `CreateBody` peut retourner `nullptr` quand la cap de 1024 corps (`Init(1024,...)` ligne 130) est atteinte. `body->GetID()` déréférence null → crash.
- **Statut Double Check :** [ ] CONFIRMÉ / [ ] RÉFUTÉ

### B21 — destroyEntity fuit le corps Jolt
- **Bug ID / Signalement :** B21 dans `tasks/CODE_REVIEW.md`
- **Diagnostic critique indépendant :** [À renseigner par @bb3d-fixer]
- **Preuve technique :** `Scene.cpp:154-166` — `destroyEntity` détruit l'entité du registry sans appeler `physics().destroyRigidBody()`. `Scene::clear()` (435-444) appelle bien `physics().clear()`, mais la destruction individuelle fuit.
- **Statut Double Check :** [ ] CONFIRMÉ / [ ] RÉFUTÉ

---

## 2. Découpage en Tâches Atomiques (Approche TDD)

### Tâche 1 : Clamper le thread count Jolt (B14)
- **Fichiers modifiés :** `src/bb3d/physics/PhysicsWorld.cpp`
- **Test unitaire associé :** `tests/unit_test_18_physics_basic.cpp` — vérifier que l'init ne crash pas avec `hardware_concurrency=0` (mock ou forçage).
- **Étape 3 (Code minimal) :** Remplacer `(int)std::thread::hardware_concurrency() - 1` par `std::max(1, (int)std::thread::hardware_concurrency() - 1)` (ligne 127).
- **Étape 5 (Commit) :** `fix(physics): clamp Jolt thread count to minimum 1 (B14)`

### Tâche 2 : Garde Transform dans createRigidBody (B15)
- **Fichiers modifiés :** `src/bb3d/physics/PhysicsWorld.cpp`
- **Étape 3 (Code minimal) :** Ajouter `if (!entity.has<TransformComponent>()) return;` en tête de `createRigidBody` (avant ligne 298).
- **Étape 5 (Commit) :** `fix(physics): guard TransformComponent in createRigidBody (B15)`

### Tâche 3 : Garde Transform dans createCharacterController (B20)
- **Fichiers modifiés :** `src/bb3d/physics/PhysicsWorld.cpp`
- **Étape 3 (Code minimal) :** Ajouter `if (!entity.has<TransformComponent>()) return;` en tête de `createCharacterController` (avant ligne 397).
- **Étape 5 (Commit) :** `fix(physics): guard TransformComponent in createCharacterController (B20)`

### Tâche 4 : Check CreateBody nullptr (B16)
- **Fichiers modifiés :** `src/bb3d/physics/PhysicsWorld.cpp`
- **Étape 3 (Code minimal) :** Après `CreateBody` (ligne 375), ajouter `if (!body) { BB_CORE_WARN("PhysicsWorld: CreateBody returned nullptr (body cap reached)"); return; }` avant `body->GetID()`.
- **Étape 5 (Commit) :** `fix(physics): check CreateBody nullptr before dereference (B16)`

### Tâche 5 : Nettoyer le corps Jolt dans destroyEntity (B21)
- **Fichiers modifiés :** `src/bb3d/scene/Scene.cpp`
- **Étape 3 (Code minimal) :** Dans `destroyEntity` (154-166), avant la destruction du registry, ajouter : `if (entity.has<PhysicsComponent>()) { m_EngineContext->physics().destroyRigidBody(entity); }`.
- **Étape 5 (Commit) :** `fix(scene): call destroyRigidBody in destroyEntity to prevent Jolt body leak (B21)`

---

## 3. Grille de Revue & Checkpoints

### 🛡️ Checkpoints de l'Implémenteur (Avant soumission en revue)
- [ ] **Double Check Bug :** Les 5 bugs (B14, B15, B20, B16, B21) confirmés dans le code source live.
- [ ] **TDD & Tests :** `ctest` au vert, pas de régression sur `unit_test_18/19`.
- [ ] **Standards C++ (`cpp-pro`) :**
  - [ ] Code, commentaires, logs en **anglais**.
  - [ ] `[[nodiscard]]` non requis ici (procédures).
- [ ] **Qualité du Build :** Zéro warning compilateur.
- [ ] **Commits :** 5 commits atomiques (`fix:`).

---

### 🔍 Checkpoints des Reviewers (Revue de Code Systématique & Approbation)
- [ ] **Double Check Validé :** La section 1.bis est renseignée pour les 5 bugs.
- [ ] **Sécurité & Robustesse :**
  - [ ] Aucun crash sur entité physique sans Transform.
  - [ ] Aucun crash quand la cap de corps est atteinte (log + skip gracieux).
  - [ ] `destroyEntity` ne fuit plus de corps Jolt.
- [ ] **Validation CTest :** Tous les tests au vert.
- [ ] **Décision Reviewer :** [ ] APPROVED | [ ] CHANGES REQUESTED
- [ ] **Historique :** 1 entrée compacte consignée dans `tasks/HISTORY.md`.

---

## 4. Journal des Échanges & Retours de Revue
- *2026-09-17* - **@agent** : Création de la fiche à partir de la revue de code indépendante du 2026-09-17.
