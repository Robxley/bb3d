# 🎯 Guide Rapide du Dossier `tasks/` (Quickstart)

Ce dossier centralise **l'ensemble du pilotage, des tâches, des revues de code et de l'historique** du moteur **biobazard3d**.  
Il s'adresse à tous les développeurs et agents IA travaillant sur le projet.

---

## 🚀 Workflow en 5 Étapes

1. **Choisir un sujet :**
   - Consulter [`ROADMAP.md`](ROADMAP.md) pour les fonctionnalités et refactorings prioritaires.
   - Consulter [`CODE_REVIEW.md`](CODE_REVIEW.md) pour les correctifs de bugs répertoriés (B1 à B28).

2. **Créer la fiche de tâche :**
   - Copier [`templates/TASK_REVIEW_TEMPLATE.md`](templates/TASK_REVIEW_TEMPLATE.md) vers `active/YYYY-MM-DD-<nom-de-la-tache>.md`.
   - Renseigner l'Auteur (`@dev`), le(s) Reviewer(s) et découper en mini-tâches atomiques TDD (2 à 5 min chacune).
   - **En cas de bug fix :** Remplir la section *1.bis Revalidation Critique du Bug* (Double Check obligatoire pour s'assurer que le bug est bien réel dans le code live).
   - Passer le statut à `[READY FOR ARCHITECTURE REVIEW]` et obtenir la validation avant de coder.

3. **Développer en TDD (Test-Driven Development) :**
   - Écrire le test unitaire de reproduction ou de nouvelle fonctionnalité d'abord (`tests/unit_test_*.cpp`).
   - Vérifier l'échec initial (RED), implémenter le code source minimal (GREEN), et valider le succès :
     ```bash
     cmake --build build --config Debug -j && ctest --test-dir build -C Debug --output-on-failure
     ```
   - Commiter de manière atomique (`feat:`, `fix:`, `refactor:`).

4. **Revue de Code Systématique & Checkpoints :**
   - L'implémenteur valide l'ensemble de ses checkpoints techniques dans la fiche.
   - Passer le statut à `[READY FOR CODE REVIEW]`.
   - **Revue systématique :** Un second agent / reviewer inspecte le patch, vérifie l'absence de régression, valide les critères de qualité (Vulkan, C++20, sécurité, perf) et signe `APPROVED`. Aucune tâche ne peut être clôturée sans cette revue formelle.

5. **Clôture & Archivage :**
   - Ajouter **1 entrée compacte (3 lignes max)** dans [`HISTORY.md`](HISTORY.md).
   - Déplacer la fiche terminée de `active/` vers `archive/`.

---

## 📂 Organisation des Sous-Dossiers

* **[`ROADMAP.md`](ROADMAP.md)** : Backlog général et vision stratégique.
* **[`CODE_REVIEW.md`](CODE_REVIEW.md)** : Rapport d'audit statique et liste des 28 bugs identifiés.
* **[`HISTORY.md`](HISTORY.md)** : Grand livre chronologique compact des chantiers terminés.
* **[`templates/`](templates/)** : Modèle officiel de tâche et grille de revue croisée.
* **[`active/`](active/)** : Tâches et revues en cours de traitement.
* **[`archive/`](archive/)** : Archives historiques de toutes les tâches complétées.
