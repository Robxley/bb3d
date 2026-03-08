---
name: planification
description: "Tu DOIS utiliser cette compétence lorsque tu as une spécification, un design ou des prérequis validés pour une tâche à plusieurs étapes, et ce, avant même de commencer à coder. Permet de générer un plan d'implémentation détaillé étape par étape."
---

# Planification (Writing Plans)

## Aperçu

Rédige des plans d'implémentation complets et détaillés. Tu dois concevoir ce plan en partant du principe qu'il sera exécuté de façon atomique (étape par étape). Documente tout ce qu'il y a à savoir : quels fichiers modifier pour chaque tâche, le code précis, la logique de test, les commandes à lancer. Découpe l'ensemble du plan en mini-tâches très granulaires. Ne te répète pas (DRY), ne fais que le nécessaire (YAGNI), et adopte une approche de Test-Driven Development (TDD) avec des commits très fréquents.

**Annonce de départ :** "J'utilise la compétence de planification pour créer le plan d'implémentation."

**Emplacement de la sauvegarde :** Sauvegarde le plan d'implémentation dans `docs/plans/YYYY-MM-DD-<nom-de-la-feature-ou-tache>.md` (adapte avec le nom du dossier de docs de ton projet s'il est différent).

## Granularité des Mini-Tâches (Bite-Sized Tasks)

Chaque étape doit correspondre à une action courte (2 à 5 minutes) :
- "Écrire le test unitaire qui échoue" (Étape 1)
- "Lancer le test pour s'assurer qu'il échoue bien" (Étape 2)
- "Implémenter le code minimal pour faire passer le test" (Étape 3)
- "Relancer le test pour s'assurer qu'il passe" (Étape 4)
- "Faire un commit" (Étape 5)

## En-tête du Document de Planification

**Chaque fichier de plan DOIT commencer par un en-tête similaire à celui-ci :**

```markdown
# Plan d'implémentation de [Nom de la Fonctionnalité]

**Objectif :** [Une phrase décrivant ce que cela construit]

**Architecture :** [2 ou 3 phrases sur l'approche technique choisie]

**Stack Technique :** [Technologies et librairies clés utilisées]

---
```

## Structure Type d'une Tâche du Plan

Chaque tâche au sein du plan doit suivre scrupuleusement le bloc de format ci-dessous :

````markdown
### Tâche N: [Nom du Composant / De la Tâche]

**Fichiers concernés :**
- Créer : `chemin/exact/vers/le/fichier.cpp`
- Modifier : `chemin/exact/vers/existant.hpp:Lignes 123-145`
- Tester : `tests/chemin/exact/test_fichier.cpp`

**Étape 1 : Écrire le test qui échoue**
```cpp
// Exemple de test très spécifique et pertinent pour la tâche
```

**Étape 2 : Lancer le test pour vérifier l'échec**
Commande : `cmake --build build && ctest -R test_name`
Attendu : Échec car la fonction n'est pas définie/comportement incorrect.

**Étape 3 : Implémentation Minimale**
```cpp
// Exemple du code source précis à ajouter/modifier pour faire passer le test
```

**Étape 4 : Lancer le test pour vérifier le succès**
Commande : `cmake --build build && ctest -R test_name`
Attendu : Succès (PASS).

**Étape 5 : Commit**
```bash
git add tests/chemin/exact/test_fichier.cpp chemin/exact/vers/le/fichier.cpp
git commit -m "feat: ajout de la fonctionnalité spécifique"
```
````

## À garder en tête pendant la rédaction

- Fournis TOUJOURS des chemins de fichiers **exacts**.
- Fournis un **code complet** dans le plan (ne dis pas juste "Ajouter une validation", écris la validation).
- Écris les **commandes de test exactes** avec le retour attendu.
- Garde l'approche TDD (Test-Driven Development) autant que possible.
- **YAGNI (You Aren't Gonna Need It)** : Garde l'implémentation au plus simple, ne prévois pas des cas extrêmes non demandés.
- Assure-toi que les étapes sont séquentiellement logiques.

## Transmission pour exécution

Une fois le plan sauvegardé, demande à l'utilisateur comment il souhaite que le code soit implémenté (exécution tâche par tâche manuellement, lancement par l'agent, etc.) et prépare-toi à lancer l'implémentation en conséquence.
