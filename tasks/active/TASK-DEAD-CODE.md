# [TASK-DEAD-CODE] : Nettoyage du code mort (D3, D4, D5, ND1)

- **Statut :** DRAFT
- **Auteur / Implémenteur :** @bb3d-fixer
- **Reviewer(s) :** @bb3d-reviewer, @dev
- **Branche Git :** `refactor/dead-code-cleanup`
- **Date de création :** 2026-09-17

---

## 1. Contexte & Objectif

Quatre zones de code mort confirmées dans le moteur :
- **D3** : `m_instanceTransforms` (`Renderer.hpp:230`, `Renderer.cpp:24,67,717`) — reserve/clear mais jamais écrit ni lu. L'instance SSBO est rempli directement depuis `m_renderCommands`.
- **D4** : `getMaterialForTexture` (`Renderer.cpp:357-364`) + `m_defaultMaterials` (`Renderer.hpp:226`) — 0 appelant externe (auto-référentiel uniquement).
- **D5** : Bloc "Horizon Culling" dans `Scene.cpp:361-399` — calcule caméra/planetRotation/faceDirections mais le corps de culling est **commenté**. Coûte du CPU par planète par frame pour rien.
- **ND1** : `StagingBuffer::submitCopy` (`StagingBuffer.cpp:49-56`) — 0 appelant, et la méthode est en plus buggée (offset incorrect).

**Objectif :**
1. Supprimer `m_instanceTransforms` et ses références.
2. Supprimer `getMaterialForTexture` et `m_defaultMaterials`.
3. Finir ou supprimer le bloc d'horizon culling commenté (décision : supprimer, car non fonctionnel).
4. Supprimer `StagingBuffer::submitCopy` (mort + buggué).
5. Valider que la compilation et CTest passent.

**Critères d'acceptation :**
- Aucune référence à `m_instanceTransforms`, `getMaterialForTexture`, `m_defaultMaterials` dans le code.
- Le bloc horizon culling est supprimé (ou réimplémenté si décision contraire).
- `submitCopy` supprimé du `.cpp` et du `.hpp`.
- `ctest` au vert, zéro warning.

---

## 1.bis Revalidation Critique du Bug (Obligatoire - Double Check)

### D3 — m_instanceTransforms mort
- **Bug ID / Signalement :** D3 dans `tasks/CODE_REVIEW.md`
- **Diagnostic critique indépendant :** [À renseigner par @bb3d-fixer]
- **Preuve technique :** `grep -rn "m_instanceTransforms"` : `Renderer.cpp:24` (reserve), `:67` (clear), `:717` (clear), `Renderer.hpp:230` (déclaration). Aucune écriture de données, aucune lecture. L'instance SSBO est rempli depuis `m_renderCommands` (Renderer.cpp:830-835).
- **Statut Double Check :** [ ] CONFIRMÉ / [ ] RÉFUTÉ

### D4 — getMaterialForTexture + m_defaultMaterials morts
- **Bug ID / Signalement :** D4 dans `tasks/CODE_REVIEW.md`
- **Diagnostic critique indépendant :** [À renseigner par @bb3d-fixer]
- **Preuve technique :** `grep -rn "getMaterialForTexture"` : seul appelant = la définition elle-même (`Renderer.cpp:357`) + déclaration (`Renderer.hpp:138`). `m_defaultMaterials` n'est utilisé que par cette fonction morte (`Renderer.cpp:68,361,363`, `Renderer.hpp:226`).
- **Statut Double Check :** [ ] CONFIRMÉ / [ ] RÉFUTÉ

### D5 — Bloc horizon culling commenté
- **Bug ID / Signalement :** D5 dans `tasks/CODE_REVIEW.md`
- **Diagnostic critique indépendant :** [À renseigner par @bb3d-fixer]
- **Preuve technique :** `Scene.cpp:361-399` — calcule `planetRotation`, itère les caméras, définit `faceDirections`, mais le corps de culling (lignes 387-397) est commenté (`/* ... */`). Le bloc ne fait rien yet coûte du CPU par planète par frame.
- **Statut Double Check :** [ ] CONFIRMÉ / [ ] RÉFUTÉ

### ND1 — StagingBuffer::submitCopy mort + buggué
- **Bug ID / Signalement :** ND1 (nouveau, découvert lors de la revue du 2026-09-17)
- **Diagnostic critique indépendant :** [À renseigner par @bb3d-fixer]
- **Preuve technique :** `grep -rn "submitCopy"` : seul appelant = la définition (`StagingBuffer.cpp:49`) + déclaration (`StagingBuffer.hpp:32`). Aucun appelant. De plus, la méthode passe `m_offset` (déjà avancé) au `copyFunc` → offset incorrect.
- **Statut Double Check :** [ ] CONFIRMÉ / [ ] RÉFUTÉ

---

## 2. Découpage en Tâches Atomiques (Approche TDD)

### Tâche 1 : Supprimer m_instanceTransforms (D3)
- **Fichiers modifiés :** `include/bb3d/render/Renderer.hpp`, `src/bb3d/render/Renderer.cpp`
- **Étape 3 (Code minimal) :** Supprimer la déclaration `m_instanceTransforms` (Renderer.hpp:230), le `reserve` (Renderer.cpp:24), et les 2 `clear` (Renderer.cpp:67, 717).
- **Étape 4 (Vérification) :** `cmake --build build --config Debug -j && ctest --test-dir build -C Debug --output-on-failure` (compile + tests).
- **Étape 5 (Commit) :** `refactor(render): remove dead m_instanceTransforms member (D3)`

### Tâche 2 : Supprimer getMaterialForTexture + m_defaultMaterials (D4)
- **Fichiers modifiés :** `include/bb3d/render/Renderer.hpp`, `src/bb3d/render/Renderer.cpp`
- **Étape 3 (Code minimal) :** Supprimer la déclaration `getMaterialForTexture` (Renderer.hpp:138), la définition (Renderer.cpp:357-364), `m_defaultMaterials` (Renderer.hpp:226), et le `clear` (Renderer.cpp:68).
- **Étape 5 (Commit) :** `refactor(render): remove dead getMaterialForTexture and m_defaultMaterials (D4)`

### Tâche 3 : Supprimer le bloc horizon culling commenté (D5)
- **Fichiers modifiés :** `src/bb3d/scene/Scene.cpp`
- **Étape 3 (Code minimal) :** Supprimer le bloc `// --- OPTIMIZATION: Horizon Culling ---` (Scene.cpp:361-399) entièrement, y compris `faceDirections`.
- **Étape 5 (Commit) :** `refactor(scene): remove commented-out horizon culling block (D5)`

### Tâche 4 : Supprimer StagingBuffer::submitCopy (ND1)
- **Fichiers modifiés :** `src/bb3d/render/StagingBuffer.cpp`, `include/bb3d/render/StagingBuffer.hpp`
- **Étape 3 (Code minimal) :** Supprimer la définition `submitCopy` (StagingBuffer.cpp:49-56) et sa déclaration (StagingBuffer.hpp:32).
- **Étape 5 (Commit) :** `refactor(render): remove dead and buggy StagingBuffer::submitCopy (ND1)`

---

## 3. Grille de Revue & Checkpoints

### 🛡️ Checkpoints de l'Implémenteur (Avant soumission en revue)
- [ ] **Double Check Bug :** Les 4 zones mortes confirmées dans le code source live.
- [ ] **TDD & Tests :** `ctest` au vert après suppression.
- [ ] **Standards C++ (`cpp-pro`) :**
  - [ ] Suppression complète (pas de `_unused` renames, pas de commentaires `// removed`).
  - [ ] Tous les call sites mis à jour.
  - [ ] Code, commentaires en **anglais**.
- [ ] **Qualité du Build :** Zéro warning compilateur.
- [ ] **Commits :** 4 commits atomiques (`refactor:`).

---

### 🔍 Checkpoints des Reviewers (Revue de Code Systématique & Approbation)
- [ ] **Double Check Validé :** La section 1.bis est renseignée.
- [ ] **Architecture :**
  - [ ] Aucune régression fonctionnelle (l'instance SSBO fonctionne toujours sans `m_instanceTransforms`).
  - [ ] Aucun include orphelin laissé après suppression.
- [ ] **Validation CTest :** Tous les tests au vert.
- [ ] **Décision Reviewer :** [ ] APPROVED | [ ] CHANGES REQUESTED
- [ ] **Historique :** 1 entrée compacte consignée dans `tasks/HISTORY.md`.

---

## 4. Journal des Échanges & Retours de Revue
- *2026-09-17* - **@agent** : Création de la fiche à partir de la revue de code indépendante du 2026-09-17.
