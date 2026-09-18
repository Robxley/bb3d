---
name: vibe-orchestrator
description: "Orchestre et délègue des tâches du projet à des agents Mistral Vibe CLI (Fixeur TDD, Reviewer en lecture seule) avec isolation Git Worktree, sélection du modèle glm-5.2 et application du double-check et de la revue systématique."
---

# Orchestrateur Mistral Vibe CLI (`vibe-orchestrator`)

Cette compétence permet à un agent principal (comme Antigravity ou un orchestrateur CI) de **déléguer l'exécution de tâches du projet à des sous-agents propulsés par Mistral Vibe CLI**.

Elle garantit le respect strict des standards du projet (`AGENTS.md`), le modèle par défaut **`glm-5.2`**, l'isolation Git par worktrees, le protocole de **Double Check critique des bugs** et la **Revue Systématique obligatoire**.

---

## 🎯 Capacités Clés

1. **Rôles Dédiés (Personas) :**
   - **`fixer` (`bb3d-fixer`) :** Implémenteur TDD. Écrit le test unitaire en premier, implémente le code C++20 / Vulkan minimal, exécute `ctest`, et met à jour la fiche `tasks/active/TASK-XXX.md`. Exécute obligatoirement le double-check critique si la tâche concerne un bug.
   - **`reviewer` (`bb3d-reviewer`) :** Relecteur d'architecture en lecture seule (`safety = safe`). Inspecte le diff, valide les critères de conformité et signe `APPROVED` ou `CHANGES REQUESTED`.
2. **Modèle par Défaut :** **`glm-5.2`** (défini dans `.vibe/agents/` et via `VIBE_ACTIVE_MODEL`).
3. **Isolation Git Native (`--worktree`) :** Permet à plusieurs agents de travailler en parallèle dans des worktrees isolés sans conflit de fichiers ou de branches.
4. **Exécution Non-Interactive & Headless :** Pilotage automatisé via `--auto-approve` et `--trust`.
5. **Traçabilité Complète :** Journalisation automatique de chaque session dans `tasks/active/logs/`.

---

## 🛠️ Utilisation par l'Orchestrateur (Commandes Prêtes à l'Emploi)

L'orchestrateur exécute le script helper Python via `run_command` :

### 1. Assigner une tâche à un Agent Fixeur (TDD)

```powershell
python .agents/skills/vibe-orchestrator/scripts/run_vibe_agent.py `
    --task tasks/active/TASK-B8-material-ubo.md `
    --role fixer `
    --worktree fix-b8 `
    --model glm-5.2
```

> **Comportement de l'agent Fixeur :**
> - Si la tâche est un bug fix : il inspecte le code, valide ou réfute l'anomalie dans la section `1.bis Revalidation Critique du Bug` (Double Check).
> - Il rédige le test de reproduction dans `tests/unit_test_*.cpp` (RED).
> - Il code le fix minimal dans `src/bb3d/` (GREEN).
> - Il valide `ctest --test-dir build -C Debug --output-on-failure`.
> - Il coche les checkpoints Implémenteur et passe le statut à `[READY FOR CODE REVIEW]`.

---

### 2. Déclencher la Revue de Code Systématique

Une fois le fix terminé, l'orchestrateur déclenche l'agent Reviewer :

```powershell
python .agents/skills/vibe-orchestrator/scripts/run_vibe_agent.py `
    --task tasks/active/TASK-B8-material-ubo.md `
    --role reviewer `
    --model glm-5.2
```

> **Comportement de l'agent Reviewer :**
> - Mode inspection pure : zéro recompilation (`cmake --build` proscrit) et zéro modification de fichier pour garantir une terminaison propre avec le **code 0**.
> - Analyse `git diff`, conformité architecturale, standards C++20 (`cpp-pro`) et Vulkan Sync2 (`vulkan-cpp`).
> - Produit un rapport de revue structuré directement dans sa réponse texte (stdout).
> - L'orchestrateur (Antigravity) réalise ensuite le **double check** indépendant du rapport, coche les checkpoints dans la fiche de tâche (`TASK-XXX.md`), et enregistre la décision.

---

### 3. En cas de Demande de Corrections (`[CHANGES REQUESTED]`)

L'orchestrateur réinjecte les retours au Fixeur en fournissant l'argument `--extra-prompt` :

```powershell
python .agents/skills/vibe-orchestrator/scripts/run_vibe_agent.py `
    --task tasks/active/TASK-B8-material-ubo.md `
    --role fixer `
    --worktree fix-b8 `
    --extra-prompt "Le reviewer demande d'ajouter un assert sur la validité du buffer frame 1/2."
```

---

### 4. Clôture de la Tâche (Quand `APPROVED`)

Une fois l'approbation obtenue :
1. Fusionner la branche du worktree dans `main` (si un worktree a été utilisé).
2. Ajouter **1 entrée compacte (3 lignes max)** dans `tasks/HISTORY.md`.
3. Déplacer la fiche vers `tasks/archive/` :
   ```powershell
   Move-Item tasks/active/TASK-B8-material-ubo.md tasks/archive/
   ```

---

## ⚙️ Configuration Projet (`.vibe/`)

Les configurations sont intégrées au dépôt pour une portabilité immédiate :
* **`.vibe/agents/bb3d-fixer.toml`** : Profil agent codeur / fixeur avec le modèle `glm-5.2`.
* **`.vibe/agents/bb3d-reviewer.toml`** : Profil agent relecteur en lecture seule avec `glm-5.2`.
* **`.vibe/prompts/bb3d_fixer_prompt.md`** : Instructions système du fixeur (règles C++20, Vulkan, TDD, Double Check).
* **`.vibe/prompts/bb3d_reviewer_prompt.md`** : Instructions système du reviewer (grille de revue de code).
