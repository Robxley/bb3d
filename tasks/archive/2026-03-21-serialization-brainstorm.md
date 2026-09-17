# Brainstorming : Stratégie de Sérialisation / Sauvegarde (Astro Bazard)

## Objectif Principal
Permettre à l'entité globale du jeu (systèmes, planètes, vaisseaux, composants de logique, paramètres) d'être sauvegardée et chargée proprement depuis/vers un fichier `json`.

## Contexte Actuel
- Le moteur possède déjà un `bb3d::SceneSerializer` basé sur `nlohmann::json`.
- Cependant, ce sérialiseur est *statique* : il ne connaît que les composants natifs du moteur (Transform, Mesh, Camera...).
- Les composants spécifiques à **Astro Bazard** (`SpaceshipController`, `CommRelay`, etc.) sont complètement ignorés lors de l'appel à `SceneSerializer::serialize()`.

## Problématisation
Pour sauvegarder l'état complet du jeu de manière idiomatique sans corrompre l'abstraction du moteur (qui ne doit pas dépendre du jeu), le flux de sérialisation doit devenir extensible.

### Choix de Conception (En attente de retour)
1. **Un Registre de Composants (Component Registry Pattern) - RECOMMANDÉ 🏆**
   - *Fonctionnement :* Modifier le moteur pour qu'il possède un registre dynamique de composants. *Astro Bazard* y enregistrera ses composants (et leurs fonctions lambdas de sérialisation associées) lors du démarrage du jeu `main.cpp`.
   - *Avantages :* Centralisé, propre, gère la création automatique d'entités avec leurs composants. C'est l'approche standard des "gros" moteurs. **Surtout**, c'est la seule méthode qui permettra à l'**Éditeur** du moteur de sauvegarder la scène correctement sans avoir à connaître le code du jeu !
   - *Performance :* Excellente (lookup de composants en O(1) via hash/type_index).
2. **Le Pattern Event Bus (Événements de Sérialisation)**
   - *Fonctionnement :* Le `SceneSerializer` du moteur émet un événement `PreSerializeEntity` et `PostDeserializeEntity` via l'`EventBus` existant du projet. La logique du jeu écoute ces événements et ajoute ses propres données JSON à la volée.
   - *Avantages :* Très léger, découplage fort. Totalement transparent pour le moteur.
   - *Inconvénients :* Overhead de l'EventBus appelé pour *chaque* entité, difficulté à gérer les erreurs ou l'ordre des composants.
3. **Le Wrapper de Sérialisation (AstroBazardSerializer)**
   - *Fonctionnement :* Ne pas toucher au moteur. Créer une nouvelle classe `AstroBazardSerializer` dans le code du jeu (Astro Bazard) qui enveloppe ou hérite de `SceneSerializer`. Il délègue d'abord la sauvegarde de la géométrie au moteur, puis parcourt tous les `SpaceshipController` / logiques pour les annexer au JSON.
   - *Avantages :* Aucune complexité ajoutée au moteur core. Simple, spécifique au jeu.
   - *Inconvénients :* **Casse l'éditeur visuel.** Si tu cliques sur "Save Scene" dans ImGui (qui tourne dans l'Engine), l'éditeur ne saura pas appeler ce Wrapper.

### Ma Recommandation : Option 1 (Le Registre)
C'est le système le plus pérenne. En quelques lignes de code côté Moteur, on crée une `std::unordered_map<std::string, ComponentSerializer>` (où ComponentSerializer contient des callbacks `serialize/deserialize/drawImGui`). Ainsi le Moteur pourra non seulement Sauvegarder/Charger les composants personnalisés de *Astro Bazard*, mais même les afficher dans l'UI de l'inspecteur sans inclure leurs headers !

Quel est ton feu vert pour partir sur l'Option 1 ?
