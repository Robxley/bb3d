# [TASK-JOBSYSTEM] : Correction du busy-poll du JobSystem (B24)

- **Statut :** DRAFT
- **Auteur / Implémenteur :** @bb3d-fixer
- **Reviewer(s) :** @bb3d-reviewer, @dev
- **Branche Git :** `fix/jobsystem-busy-poll-b24`
- **Date de création :** 2026-09-17

---

## 1. Contexte & Objectif

`JobSystem.cpp:104-108` — le prédicat du `m_globalCondition.wait_for(lock, st, 1ms, [&]{ return false; })` est **toujours faux**. Les workers timeout systématiquement après 1 ms peu importe le `notify_all`. La condition variable ne se réveille jamais sur du vrai travail ; c'est du busy-poll à 1 kHz par worker. Le `notify_all` de `pushInternal` (ligne 59) est gaspillé.

**Impact performance :** ~1000 wakeups/sec/worker de polling inutile (P7 dans `CODE_REVIEW.md`).

**Objectif :**
1. Confirmer le bug dans le code live (Double Check).
2. Corriger le prédicat pour qu'il vérifie un flag "jobs disponibles" positionné sous `m_globalMutex` avant `notify`.
3. Valider que les workers se réveillent sur du vrai travail et non sur timeout.
4. Valider via CTest.

**Critères d'acceptation :**
- Le prédicat du `wait_for` retourne `true` quand des jobs sont disponibles.
- Le `notify_all` réveille effectivement les workers en attente.
- Pas de busy-poll à 1 kHz.
- `ctest` au vert.

---

## 1.bis Revalidation Critique du Bug (Obligatoire - Double Check)
- **Bug ID / Signalement :** B24 dans `tasks/CODE_REVIEW.md`
- **Diagnostic critique indépendant :** [À renseigner par @bb3d-fixer]
- **Preuve technique / Scénario de panne :** `JobSystem.cpp:104-108` — `m_globalCondition.wait_for(lock, st, 1ms, [&]{ return false; })`. Le prédicat `return false` est hardcoded → le `wait_for` ne peut jamais retourner sur notification, uniquement sur timeout (1 ms). Chaque worker se réveille 1000 fois/sec pour rien. Le `notify_all` de `pushInternal` (ligne 59) est gaspillé car le prédicat ne vérifie jamais l'état des queues.
- **Statut Double Check :** [ ] CONFIRMÉ / [ ] RÉFUTÉ

---

## 2. Découpage en Tâches Atomiques (Approche TDD)

### Tâche 1 : Corriger le prédicat du wait_for (B24)
- **Fichiers modifiés :** `src/bb3d/core/JobSystem.cpp`
- **Test unitaire associé :** `tests/unit_test_08_core_systems.cpp` (existant) — étendre pour vérifier que les jobs sont exécutés rapidement (pas de latence de 1 ms).
- **Étape 1 (Test) :** Étendre `unit_test_08` pour pousser un job et vérifier qu'il s'exécute sans délai excessif (mesure du temps de réveil).
- **Étape 2 (Vérification échec) :** `cmake --build build --config Debug -j && ctest --test-dir build -C Debug -R core_systems --output-on-failure` (RED — latence élevée).
- **Étape 3 (Code minimal) :**
  - Remplacer le prédicat `return false` par une vérification réelle : `return !m_globalQueue.empty() || <autres conditions de job disponible>`.
  - S'assurer que le flag/état vérifié par le prédicat est positionné sous `m_globalMutex` **avant** le `notify_all` dans `pushInternal`.
  - Vérifier la cohérence avec les queues locales et le work-stealing.
- **Étape 4 (Vérification succès) :** `ctest` (PASS / GREEN — réveil réactif).
- **Étape 5 (Commit) :** `fix(jobsystem): correct wait_for predicate to wake on available jobs instead of busy-poll (B24)`

---

## 3. Grille de Revue & Checkpoints

### 🛡️ Checkpoints de l'Implémenteur (Avant soumission en revue)
- [ ] **Double Check Bug :** B24 confirmé dans le code source live.
- [ ] **TDD & Tests :** `unit_test_08` étendu et validé + `ctest` complet au vert.
- [ ] **Standards C++ (`cpp-pro`) :**
  - [ ] Zéro allocation dynamique dans le *Hot Path*.
  - [ ] Code, commentaires, logs en **anglais**.
  - [ ] Thread-safety : le prédicat et le `notify` sont sous le même mutex.
- [ ] **Qualité du Build :** Zéro warning compilateur.
- [ ] **Commits :** Commit atomique (`fix:`).

---

### 🔍 Checkpoints des Reviewers (Revue de Code Systématique & Approbation)
- [ ] **Double Check Validé :** La section 1.bis est renseignée.
- [ ] **Concurrence & Thread-safety :**
  - [ ] Le prédicat vérifie l'état des queues sous `m_globalMutex`.
  - [ ] Le `notify_all` est appelé sous `m_globalMutex` après positionnement du flag.
  - [ ] Pas de race entre le prédicat et l'ajout de jobs.
  - [ ] Pas de deadlock (le mutex est relâché pendant `wait_for`).
- [ ] **Performance :**
  - [ ] Les workers ne se réveillent plus toutes les 1 ms (vérifier via profiling ou log).
- [ ] **Validation CTest :** Tous les tests au vert.
- [ ] **Décision Reviewer :** [ ] APPROVED | [ ] CHANGES REQUESTED
- [ ] **Historique :** 1 entrée compacte consignée dans `tasks/HISTORY.md`.

---

## 4. Journal des Échanges & Retours de Revue
- *2026-09-17* - **@agent** : Création de la fiche à partir de la revue de code indépendante du 2026-09-17.
