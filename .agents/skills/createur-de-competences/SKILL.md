---
name: createur-de-competences
description: Utilise cette compétence à chaque fois qu'il faut concevoir, structurer et générer de nouvelles compétences (skills) pour l'agent. Elle te guide pour créer des compétences claires, cohérentes et respectant le standard officiel d'Antigravity.
---

# Créateur de compétences

Tu agis en tant qu'expert en création de compétences (skills) pour le système Antigravity. Ton but est de générer rapidement des compétences claires, cohérentes et directement utilisables, en respectant les bonnes pratiques officielles. 

**RÈGLE ABSOLUE :** À chaque fois que tu utilises cette compétence pour créer une nouvelle compétence, TOUT doit être écrit en français (titres, descriptions, instructions).

## 1. Nature & Emplacement des Compétences
- **Définition** : Une compétence est un dossier réutilisable contenant des connaissances et des instructions pour une tâche spécialisée.
- **Emplacements autorisés** :
  - **Workspace (Projet spécifique)** : `<workspace-root>/.agent/skills/<nom-du-dossier>/`
  - **Global (Tous les projets)** : `~/.gemini/antigravity/skills/<nom-du-dossier>/` (Sur Windows, utilise le chemin absolu `C:\Users\<User>\.gemini\antigravity\skills\...`)

## 2. Le fichier obligatoire `SKILL.md`
Chaque dossier de compétence **DOIT obligatoirement** contenir un fichier nommé `SKILL.md`.
Ce fichier **doit obligatoirement** commencer par un bloc de métadonnées YAML (frontmatter) contenant `name` et `description`.

### Format requis du frontmatter :
```yaml
---
name: nom-de-la-competence
description: Description courte et claire à la troisième personne avec des mots-clés. Cette description est cruciale car elle permet à l'agent de découvrir et de choisir cette compétence automatiquement selon le contexte.
---
```

*(Note : Le reste du fichier `SKILL.md` contiendra les instructions détaillées en Markdown.)*

## 3. Structure recommandée du dossier (Optionnel)
En plus du `SKILL.md`, le dossier de la compétence peut contenir :
- `scripts/` : Scripts de ligne de commande ou utilitaires pour aider l'agent.
- `examples/` : Implémentations de référence ou exemples de code pour montrer à l'agent comment faire.
- `resources/` : Fichiers templates, assets ou documentation supplémentaire que la compétence peut référencer.

## 4. Bonnes Pratiques de Conception
Pour qu'une compétence soit efficace pour l'agent, tu dois respecter ces principes :
1. **Portée ciblée (Focused Scope)** : Une compétence doit exceller dans un seul type de tâche spécifique plutôt que d'être trop globale.
2. **Descriptions optimisées pour la découverte** : Rédige le `description` du YAML avec soin, en incluant les mots-clés que l'utilisateur pourrait employer.
3. **Scripts "Boîte Noire"** : Si tu fournis des scripts dans `scripts/`, ordonne à l'agent de les exécuter avec le drapeau `--help` pour comprendre leur fonctionnement, plutôt que de l'obliger à lire leur code source. Cela économise la fenêtre de contexte.
4. **Arbres de décision** : Pour les tâches complexes, structure le `SKILL.md` avec une section claire de type "Comment choisir la meilleure approche", agissant comme un arbre de décision pour guider l'agent face à divers scénarios.

## 5. Processus de création étape par étape
Lorsque l'utilisateur te demande de créer une nouvelle compétence, suis systématiquement ce flux :
1. **Analyse de la demande** : Comprends l'objectif de la compétence. Pose des questions si le périmètre est flou.
2. **Définition du nom et de l'emplacement** : Détermine s'il s'agit d'une compétence globale ou de workspace, et choisis un nom de dossier explicite (ex: `revue_de_code`).
3. **Rédaction** : Rédige le contenu du `SKILL.md` en français, en n'oubliant surtout pas le bloc YAML au début. Structure le document avec des titres `#`, `##` clairs.
4. **Génération** : Utilise l'outil `write_to_file` pour créer l'arborescence et écrire le `SKILL.md`. Si le dossier n'existe pas, l'outil le créera automatiquement.
5. **Validation** : Vérifie que le fichier a bien été généré à l'emplacement prévu.
6. **Notification** : Appelle l'outil `notify_user` pour annoncer la création de la compétence, et détaille brièvement ce qu'elle contient.
