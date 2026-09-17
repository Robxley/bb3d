# [TASK-B8] : Résolution de la course CPU/GPU sur l'UBO Matériau (Triple Buffering)

- **Statut :** [APPROVED]
- **Auteur / Implémenteur :** @bb3d-fixer
- **Reviewer(s) :** @bb3d-reviewer, @Antigravity
- **Branche Git :** `fix/b8-material-ubo`
- **Date de création :** 2026-09-17

---

## 1. Contexte & Objectif

Dans `tasks/CODE_REVIEW.md`, le bug **B8** (gravité HIGH) indique :
> `Material::SetCurrentFrame()` est déclaré (`Material.hpp:63`) mais n'est **jamais appelé** dans le moteur (`Renderer`, `Scene`, `Engine`).
> Par conséquent, la variable statique `Material::s_currentFrame` reste indéfiniment à 0.
> Lors de `Material::getDescriptorSet()`, seul `m_sets[0]` est interrogé et alloué. Les entrées `m_sets[1]` et `m_sets[2]` ne sont jamais utilisées.
> En présence de `MAX_FRAMES_IN_FLIGHT = 3`, le même buffer UBO et le même descriptor set sont réécrits par le CPU à chaque frame pendant que le GPU lit encore potentiellement la frame précédente, provoquant une **course CPU/GPU sur les paramètres de matériau**.

**Objectif du correctif :**
1. Valider de façon critique la présence effective de l'anomalie dans le code source actuel (Double Check).
2. Concevoir un test unitaire dans `tests/` démontrant la synchronisation de frame et l'allocation des sets de triple-buffering.
3. Appeler `Material::SetCurrentFrame(m_currentFrame)` au début du cycle de rendu dans `Renderer` avant toute mise à jour ou liaison de matériau.
4. Valider l'exécution des tests CTest sans warning ni régression.

---

## 1.bis Revalidation Critique du Bug (Obligatoire - Double Check)
- **Bug ID / Signalement :** B8 dans `tasks/CODE_REVIEW.md` (`Material.hpp:63, 67`, `Renderer.cpp`)
- **Diagnostic critique indépendant :** [À renseigner par @bb3d-fixer après inspection du code live]
- **Preuve technique / Scénario de panne :** [À renseigner par @bb3d-fixer]
- **Statut Double Check :** [ ] CONFIRMÉ (Bug réel et reproductible) / [ ] RÉFUTÉ (Faux positif argumenté)

---

## 2. Découpage en Tâches Atomiques (Approche TDD)

### Tâche 1 : Test unitaire et reproduction de l'indexation per-frame
- **Fichiers cibles :** `tests/unit_test_material.cpp` (ou test équivalent), `include/bb3d/render/Material.hpp`
- **Étape 1 (Test) :** Écrire un test unitaire vérifiant que `Material::SetCurrentFrame(frame)` commute bien le descriptor set / param buffer courant pour `frame = 0, 1, 2`.
- **Étape 2 (Vérification) :** `cmake --build build --config Debug -j && ctest --test-dir build -C Debug --output-on-failure`
- **Étape 3 (Code minimal) :** Appeler `Material::SetCurrentFrame(m_currentFrame)` dans `Renderer::render()` / `drawScene()`.
- **Étape 4 (Vérification succès) :** Validation `ctest` (PASS).
- **Étape 5 (Commit) :** Commit atomique `fix(render): call Material::SetCurrentFrame to fix UBO race condition (B8)`.

---

## 3. Grille de Revue & Checkpoints

### 🛠️ Checkpoints de l'Implémenteur (Avant soumission en revue)
- [ ] **Double Check Bug :** L'existence du bug a été vérifiée de manière critique et confirmée dans le code source avant toute modification (pas de faux positif).
- [ ] **TDD & Tests :** Les nouveaux tests et les tests unitaires existants passent (`ctest`).
- [ ] **Standards C++ (`cpp-pro`) :**
  - [ ] Zéro allocation dynamique dans le *Hot Path* (`render()` / `update()`).
  - [ ] `std::span` et `std::string_view` utilisés pour le passage de paramètres (Zero-Copy).
  - [ ] Initialisation désignée C++20 (`Type{.field = val}`).
  - [ ] `[[nodiscard]]` présent sur les accesseurs et fonctions critiques.
  - [ ] Code, commentaires, logs et documentation Doxygen rédigés en **anglais**.
- [ ] **Standards Vulkan (`vulkan-cpp`) :**
  - [ ] Triple buffering des UBOs et descriptor sets effectif sur les 3 frames in flight.
  - [ ] Aucune fuite de Descriptor Sets.
- [ ] **Qualité du Build :** Zéro warning compilateur (`/W4` sous MSVC).
- [ ] **Commits :** Commits atomiques et messages de commit clairs (`fix:`).

---

### 🔍 Checkpoints des Reviewers (Revue de Code Systématique & Approbation)
- [ ] **Double Check Validé :** La section 1.bis est dûment renseignée et démontrée.
- [ ] **Architecture & Opacité (`AGENTS.md`) :**
  - [ ] Pas de fuite de types Vulkan dans les interfaces publiques.
  - [ ] Appel placé au bon endroit dans le cycle de frame du Renderer.
- [ ] **Sécurité & Robustesse :**
  - [ ] Pas de risque d'accès hors-limites (`currentFrame >= MAX_FRAMES_IN_FLIGHT`).
- [ ] **Validation CTest :**
  - [ ] Tous les tests automatisés au vert.
- [ ] **Décision Reviewer :**
  - [ ] **APPROVED** (Prêt pour la fusion)
  - [ ] **CHANGES REQUESTED** (Voir commentaires ci-dessous)
- [ ] **Historique :** 1 entrée compacte consignée dans `tasks/HISTORY.md`.

---

## 4. Journal des Échanges & Retours de Revue
- *2026-09-17* - **@Antigravity** : Initialisation de la tâche et assignation à l'agent Vibe `@bb3d-fixer`.
