# Plan d'Amélioration des Cascaded Shadow Maps (CSM)

## Contexte
Les Cascaded Shadow Maps (CSM) sont une technique essentielle pour le rendu des ombres en temps réel dans les environnements 3D. L'implémentation actuelle dans le moteur bb3d suit les bonnes pratiques, mais des améliorations peuvent être apportées pour réduire les artefacts et optimiser les performances.

## Objectifs
1. **Réduire les artefacts** : Améliorer la qualité des ombres en réduisant les artefacts comme le "shadow acne" et les coutures entre cascades.
2. **Optimiser les performances** : Réduire le coût de rendu des cascades, notamment en évitant le rendu redondant.
3. **Améliorer la stabilité** : Utiliser des techniques avancées pour la distribution des splits et la gestion des ombres.

## Plan d'Action

### 1. Amélioration de la Distribution des Splits
**Problème** : La distribution actuelle utilise une combinaison de distributions logarithmique et linéaire, mais pourrait être optimisée pour une meilleure stabilité.

**Solution** :
- **Implémenter la distribution pratique (Practical Split Distribution)** : Cette méthode ajuste dynamiquement les splits en fonction de la scène pour une meilleure répartition de la résolution.
- **Ajouter un paramètre de contrôle** : Permettre aux utilisateurs de choisir entre différentes méthodes de distribution (logarithmique, linéaire, pratique).

**Fichiers à modifier** :
- `src\bb3d\render\ShadowCascade.cpp` : Mettre à jour la fonction `calculateSplitDistances`.
- `include\bb3d\render\ShadowCascade.hpp` : Ajouter un énuméré pour les méthodes de distribution.

### 2. Réduction des Artefacts
**Problème** : Des artefacts comme le "shadow acne" et les coutures entre cascades peuvent apparaître.

**Solution** :
- **Ajouter un Normal Bias** : Utiliser un bias basé sur la normale pour réduire les artefacts de "shadow acne".
- **Améliorer le Texel Snapping** : Optimiser le texel snapping pour une meilleure stabilité des ombres.
- **Utiliser un Bias Adaptatif** : Ajuster dynamiquement le bias en fonction de la distance de la cascade.

**Fichiers à modifier** :
- `src\bb3d\render\ShadowCascade.cpp` : Mettre à jour la fonction `calculateLightSpaceMatrix`.
- `src\bb3d\render\Renderer.cpp` : Ajouter un calcul de bias adaptatif.

### 3. Optimisation des Performances
**Problème** : Le rendu de plusieurs cascades peut être coûteux en performance.

**Solution** :
- **Culling des Objets** : Implémenter un culling des objets hors du frustum de chaque cascade pour éviter le rendu redondant.
- **Réduction de la Résolution** : Utiliser une résolution plus faible pour les cascades éloignées.
- **Parallelisation** : Utiliser le système de jobs pour paralléliser le rendu des cascades.

**Fichiers à modifier** :
- `src\bb3d\render\Renderer.cpp` : Ajouter un culling des objets et optimiser le rendu des cascades.
- `src\bb3d\core\JobSystem.cpp` : Utiliser le système de jobs pour paralléliser le rendu.

### 4. Amélioration de la Documentation
**Problème** : La documentation actuelle est limitée et ne couvre pas les choix de conception.

**Solution** :
- **Ajouter des commentaires détaillés** : Expliquer les choix de conception, notamment sur la valeur de `lambda` et son impact sur la qualité des ombres.
- **Créer un guide d'utilisation** : Documenter les meilleures pratiques pour configurer les CSM dans le moteur.

**Fichiers à modifier** :
- `include\bb3d\render\ShadowCascade.hpp` : Ajouter des commentaires détaillés.
- `docs\CSM_Guide.md` : Créer un guide d'utilisation des CSM.

## Étapes de Mise en Œuvre

### Phase 1 : Recherche et Planification
- **Recherche** : Étudier les techniques avancées de CSM et leurs implémentations.
- **Planification** : Définir les étapes détaillées pour chaque amélioration.

### Phase 2 : Implémentation
- **Amélioration de la Distribution des Splits** : Implémenter la distribution pratique et ajouter un paramètre de contrôle.
- **Réduction des Artefacts** : Ajouter un normal bias et améliorer le texel snapping.
- **Optimisation des Performances** : Implémenter le culling des objets et utiliser le système de jobs.

### Phase 3 : Tests et Validation
- **Tests Unitaires** : Créer des tests unitaires pour valider les améliorations.
- **Tests de Performance** : Mesurer l'impact des optimisations sur les performances.
- **Validation Visuelle** : Vérifier la qualité des ombres et l'absence d'artefacts.

### Phase 4 : Documentation
- **Commentaires** : Ajouter des commentaires détaillés dans le code.
- **Guide d'Utilisation** : Créer un guide d'utilisation des CSM.

## Conclusion
Ce plan d'action vise à améliorer la qualité et les performances des Cascaded Shadow Maps dans le moteur bb3d. En suivant ces étapes, nous pouvons réduire les artefacts, optimiser les performances et fournir une meilleure expérience utilisateur.