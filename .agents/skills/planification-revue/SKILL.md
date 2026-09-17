---
name: planification-revue
description: "Standard de planification, exécution TDD et revue de code multi-développeurs/agents avec checkpoints croisés (Implémenteur vs Reviewer), validation Vulkan et critères de fusion."
---

# Planification & Revue de Code Organisée (planification-revue)

Cette compétence régit le **standard officiel et obligatoire** de planification technique, de suivi d'exécution TDD et de revue de code pour l'ensemble des développeurs et agents travaillant sur le moteur **biobazard3d**.

---

## 1. Principes & Emplacement des Fichiers

- **Règle Fondamentale :** Aucun plan technique ni suivi de tâche ne doit être stocké dans le dossier `docs/` (qui est strictement réservé à la documentation technique finale du moteur).
- **Emplacement des Tâches Actives :** Toutes les tâches en cours sont créées dans :
  `tasks/active/YYYY-MM-DD-<nom-de-la-tache>.md`
- **Modèle de Référence :** Chaque nouvelle tâche doit être initialisée à partir du template :
  `tasks/templates/TASK_REVIEW_TEMPLATE.md`
- **Archivage :** Une fois la tâche terminée, validée par les reviewers et fusionnée, son fichier `.md` est déplacé dans :
  `tasks/archive/`

---

## 2. Cycle de Vie d'une Tâche

Chaque document de tâche doit afficher et mettre à jour son **Statut** au fil de l'eau :

```
[DRAFT] ──────> [READY FOR ARCHITECTURE REVIEW] ──────> [APPROVED]
                                                            │
                                                            ▼
[DONE] <────── [READY FOR CODE REVIEW] <────── [IN PROGRESS]
  ▲                     │
  │                     ▼
  └─────────── [CHANGES REQUESTED]
```

1. **`[DRAFT]` :** L'auteur (`Assignee`) rédige le contexte, l'architecture et le découpage en tâches atomiques TDD.
2. **`[READY FOR ARCHITECTURE REVIEW]` :** Soumission aux reviewers (`Reviewers`) pour validation de la conception avant tout codage.
3. **`[APPROVED]` :** Les relecteurs ont approuvé le plan. Le développement peut commencer.
4. **`[IN PROGRESS]` :** L'implémenteur réalise les tâches pas à pas selon l'approche TDD (test unitaire -> échec -> code minimal -> succès -> commit atomique).
5. **`[READY FOR CODE REVIEW]` :** L'implémenteur a validé l'ensemble de ses checkpoints techniques et sollicite la revue de code.
6. **`[CHANGES REQUESTED]` :** Un ou plusieurs reviewers demandent des ajustements (documentés dans le Journal des Échanges). L'implémenteur applique les corrections.
7. **`[DONE]` :** Tous les checkpoints sont cochés (`[x]`), validation layers propres, suite de tests au vert. **Obligation :** Consigner 1 entrée compacte (3 lignes) dans `tasks/HISTORY.md`. Le fichier est ensuite déplacé dans `tasks/archive/`.


---

## 3. Grille de Checkpoints à Vérifier

### 🛠️ Checkpoints de l'Implémenteur (Avant soumission en revue)
- [ ] **TDD & Tests :** Les nouveaux tests et les tests de régression existants passent (`ctest`).
- [ ] **Standards C++ (`cpp_pro`) :**
  - [ ] Zéro allocation dynamique dans le *Hot Path* (`render()` / `update()`).
  - [ ] `std::span` et `std::string_view` utilisés pour le passage de paramètres (Zero-Copy).
  - [ ] Initialisation désignée C++20 (`Type{.field = val}`).
  - [ ] `[[nodiscard]]` présent sur les accesseurs et fonctions critiques.
  - [ ] Code, commentaires, logs et documentation Doxygen rédigés en **anglais**.
- [ ] **Standards Vulkan (`vulkan-cpp`) :**
  - [ ] Synchronisation moderne : `pipelineBarrier2` avec `vk::DependencyInfo` (aucun appel à `pipelineBarrier` legacy).
  - [ ] Zéro attente bloquante CPU (`waitIdle()`) pour les uploads.
  - [ ] Aucune fuite de Descriptor Sets (libération RAII vérifiée).
  - [ ] Dynamic Rendering sans RenderPass legacy.
- [ ] **Qualité du Build :** Zéro warning compilateur (`/W4` sous MSVC, `-Wall -Wextra` sous Clang/GCC).
- [ ] **Commits :** Commits atomiques et messages de commit clairs (`feat:`, `fix:`, `refactor:`).

---

### 🔍 Checkpoints des Reviewers (Validation & Approbation)
- [ ] **Architecture & Opacité (`GEMINI.md`) :**
  - [ ] Les types Vulkan (`vk::*`) restent 100% opaques vis-à-vis du code utilisateur/scene.
  - [ ] Respect de la séparation CPU/GPU (pas de transfert inutile par frame).
  - [ ] Multi-streams sommets respecté (pas d'Uber-Vertex sur les passes d'ombres/picking).
- [ ] **Sécurité & Robustesse :**
  - [ ] Vérifications lourdes correctement isolées sous `#if defined(BB3D_DEBUG)`.
  - [ ] Gestion des cas limites (redimensionnement à 0x0, textures manquantes, etc.).
- [ ] **Validation GPU & Profiling :**
  - [ ] Validation Layers Khronos sans erreur ni warning.
  - [ ] Absence de régressions de framerate ou de pics mémoire inattendus.
- [ ] **Décision Reviewer :**
  - [ ] **APPROVED** (Prêt pour la fusion)
  - [ ] **CHANGES REQUESTED** (Commentaires détaillés dans le journal)

---

## 4. Instructions d'Exécution pour l'Agent

1. **Création :** À chaque nouvelle fonctionnalité ou refactoring conséquent, copier `tasks/templates/TASK_REVIEW_TEMPLATE.md` vers `tasks/active/YYYY-MM-DD-<sujet>.md`.
2. **Attribution :** Renseigner clairement l'Auteur (`@nom_dev` ou `@agent_name`) et le(s) Reviewer(s).
3. **Granularité TDD :** Rédiger des mini-tâches de 2 à 5 minutes avec le test unitaire exact, les commandes CMake/CTest associées et les fichiers cibles.
4. **Validation Croisée :** Ne jamais marquer une tâche `DONE` sans avoir validé les cases de l'implémenteur ET obtenu l'approbation formelle des reviewers.
5. **Historique & Archivage :** Dès que la tâche est validée, consigner 1 entrée compacte dans `tasks/HISTORY.md` (Date, ID, Scope, Auteur/Reviewer, Tests, Archive), puis déplacer le fichier de `tasks/active/` vers `tasks/archive/`.

