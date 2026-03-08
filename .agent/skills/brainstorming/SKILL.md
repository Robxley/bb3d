---
name: brainstorming
description: "Tu DOIS utiliser cette compétence avant tout travail créatif (création de fonctionnalités, ajout de composants ou modification de comportement). Permet d'explorer l'intention de l'utilisateur, les prérequis et le design avant de commencer l'implémentation."
---

# Brainstorming : Transformer les Idées en Designs

## Aperçu

Cette compétence t'aide à transformer des idées en designs et spécifications complets via un dialogue collaboratif naturel avec l'utilisateur.

Commence par comprendre le contexte actuel du projet, puis pose des questions une par une pour affiner l'idée. Une fois que tu as compris ce que tu dois construire, présente le design et obtiens l'approbation de l'utilisateur.

**RÈGLE STRICTE :** Ne lance aucune compétence d'implémentation, n'écris aucun code, ne crée aucune structure de projet et ne prends aucune action d'implémentation avant d'avoir présenté un design complet et obtenu l'approbation de l'utilisateur. Cela s'applique à CHAQUE tâche concernée, quelle que soit sa simplicité apparente.

## Anti-Pattern : "C'est trop simple pour nécessiter un design"

Chaque tâche doit passer par ce processus. Qu'il s'agisse d'une to-do list, d'un utilitaire basique ou d'une modification de configuration. Les tâches "simples" sont souvent celles où les hypothèses non vérifiées entraînent le plus de pertes de temps. Le design peut être très court (quelques phrases), mais tu DOIS le présenter et obtenir une validation.

## Liste de Contrôle (Checklist)

Tu dois créer une tâche pour chacun de ces points et les compléter dans l'ordre :

1. **Explorer le contexte du projet** : Vérifier les fichiers, la documentation, les commits récents.
2. **Poser des questions de clarification** : Une à la fois pour bien comprendre le but, les contraintes et les critères de réussite.
3. **Proposer 2 à 3 approches** : Présenter les avantages/inconvénients et ta recommandation.
4. **Présenter le design** : Par sections adaptées à la complexité, en demandant l'approbation de l'utilisateur après chaque section.
5. **Écrire le document de conception (Design Doc)** : Sauvegarder dans `docs/plans/YYYY-MM-DD-<sujet>-design.md` (adapter si un autre dossier de docs existe).
6. **Transition vers l'implémentation** : Invoquer la compétence de `planification` pour créer le plan d'implémentation.

## Le Processus en Détail

**1. Comprendre l'idée :**
- Vérifie l'état actuel du projet d'abord (fichiers, documentation, structure).
- Pose des questions une par une pour affiner l'idée de l'utilisateur.
- Préfère les questions à choix multiples (QCM) lorsque c'est possible, mais des questions ouvertes sont acceptables.
- Ne pose qu'UNE SEULE question par message.
- Concentre-toi sur la compréhension du but, des contraintes et des objectifs.

**2. Explorer les approches :**
- Propose 2 à 3 approches d'architecture ou de résolution différentes.
- Présente les options de façon conversationnelle en mettant en avant ta recommandation principale et le raisonnement derrière.

**3. Présenter le design :**
- Une fois que tu as compris ce qui doit être construit, présente le design.
- Adapte la taille de chaque section à sa complexité (quelques phrases si c'est très simple, jusqu'à 200 mots si c'est complexe).
- Pose la question après chaque section pour savoir si l'utilisateur est d'accord jusqu'ici.
- Couvre : l'architecture, les composants, le flux de données, la gestion des erreurs et les tests.

## Après le Design

**Documentation :**
- Rédige le design tel que validé dans le dossier cible (généralement `docs/plans/...-design.md` ou dans un path défini par le projet).

**Implémentation :**
- Invoque immédiatement la compétence `planification` pour créer le plan d'implémentation détaillé. Ne lance aucune autre compétence.
