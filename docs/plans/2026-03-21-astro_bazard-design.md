# Astro Bazard - Game Design Document (GDD)

*Date : 21 Mars 2026*

## 1. Vision Globale : "FTL vs OGame en Gravité Newtonienne"
Astro Bazard est un jeu de gestion, d'exploration et de survie spatiale en 2D procédurale. 
L'intégralité du gameplay, du déplacement des vaisseaux, de la physique et de la construction planétaire **évolue strictement sur un plan 2D** pour le moment (avec un potentiel déblocage de l'axe 3D (Z) dans de futures mises à jour). 

Le joueur contrôle un vaisseau soumis à une physique képlérienne exigeante (inertie, fronde gravitationnelle, puits de gravité). Le cœur du jeu repose sur un équilibre constant entre **l'exploration risquée à bord du vaisseau (façon Faster Than Light)** et le **développement/la défense de colonies planétaires (façon Ogame / Tower Defense)**.

---

## 2. Piliers de Gameplay

### A. Pilotage Sensible & Vaisseau FTL-like
- **Physique Environnementale** : Le vaisseau navigue dans un environnement vivant. L'attraction des planètes, les tempêtes solaires (qui repoussent le vaisseau), et les ceintures d'astéroïdes obligent à un pilotage précis.
- **Vaisseau Modulaire** : Le vaisseau dispose de "slots" énergétiques (Réacteur, Propulseurs, Boucliers, Module de minage/cargo, Armement). 
- **Événements Spatiaux & Micro-Gestion** : Comme dans FTL, des pirates ou des orages magnétiques peuvent endommager des systèmes spécifiques du vaisseau. Le joueur doit allouer la puissance d'urgence vers les propulseurs pour fuir ou vers l'armement pour riposter.

### B. Colonisation & Développement (Le volet OGame)
- **Exploitation des Ressources** : Extraire des minerais rares d'astéroïdes ou forer à la surface des planètes.
- **Grille de Construction Planétaire** : Poser son vaisseau sur une planète permet de fonder une colonie. On y construit des bâtiments (Extracteurs de métal/cristal, Silos, Centrales énergétiques).
- **Croissance** : Ces planètes génèrent des ressources sur la durée, servant à crafter de meilleures pièces de vaisseau ou à fortifier d'autres colonies.

### C. Tower Defense Atmosphérique & Orbitale
- **Bâtiments Destructibles** : Les bâtiments planétaires sont des entités physiques (RigidBody 2D).
- **Gestion de l'Énergie (Délestage)** : Tout comme dans FTL pour le vaisseau, la planète possède une grille d'alimentation. Lors d'une attaque, le joueur doit désactiver temporairement les mines ou usines pour rediriger 100% de la puissance vers les boucliers ou les lasers.
- **Grille Orbitale** : En plus des tourelles au sol, le joueur peut lancer des satellites de défense et des drones. Ils sont soumis à la gravité et évoluent physiquement en orbite, créant un réseau de défense dynamique (mais vulnérable aux collisions avec les météores).
- **Menaces Extérieures** : 
    - *Pluies de météorites* : Des astéroïdes sont attirés par la pesanteur de la planète et chutent vers la surface.
    - *Raids Pirates* : Des vaisseaux ennemis approchent pour piller la colonie.
- **Défense** : Le joueur construit des Tourelles de Défense, des Boucliers Dômes et des Lasers anti-météores. Le système de Tower Defense gère la priorisation des cibles (un gros astéroïde destructeur vs un petit chasseur pirate rapide).

### D. Logistique & Le Réseau "COMM-LINK"
- **Ubiquité** : La caméra ne suit pas uniquement le vaisseau. Dès qu'une planète possède un "Relais de Communication", le joueur peut basculer la vue (Smart Camera) du vaisseau vers cette colonie instantanément.
- **Transport Automatisé** : Une fois des routes sécurisées entre deux colonies, le joueur peut affecter des "Drones Cargo". Ceux-ci transportent physiquement les ressources (Métal, Cristaux) d'une planète à l'autre. Le joueur doit intervenir avec son vaisseau de combat si un convoi est intercepté par des pirates.
- **Multi-tâche** : Pendant que le vaisseau dérive en route vers une lointaine anomalie stellaire, le joueur peut surveiller sa colonie, rediriger l'énergie lors d'un raid, ou observer le transit de ses cargos sécurisés.

### E. Événements Aléatoires & Rencontres
Le voyage dans le vide interstellaire est ponctué de :
- **Anomalies Scientifiques** : Des zones étranges qui offrent de la technologie ancienne mais perturbent le radar ou la gravité.
- **Marchands Itinérants** : La possibilité d'échanger des matières premières contre des "Blueprints" (schémas de construction) uniques de tourelles ou de pièces de vaisseau.

---

## 3. Boucle de Jeu (Game Loop)

1. **🚀 Décollage & Survie** : Quitter l'orbite initiale, affronter un champ d'astéroïdes physique, gérer son carburant.
2. **🪐 Découverte** : Scanner et atterrir sur une planète riche en ressources.
3. **🛠️ Fondation** : Établir la première "Tour de Com" et des extracteurs. 
4. **⚔️ Fortification** : Détecter une pluie de météorites entrante ; utiliser les maigres ressources récoltées pour ériger un barrage de lasers anti-astéroïdes (Tower Defense).
5. **🔭 Expansion** : Reprendre le vaisseau vers le système suivant, tout en commutant la caméra pour surveiller la première base livrée à elle-même.

---

## 4. Faisabilité & Architecture (`bb3d`)

- **Contrainte Physique 2D Stricte** : Les menaces (Météores) et le Vaisseau utilisent le `PhysicsWorld` (Jolt) existant. Le composant PhysicsComponent doit avoir `constrain2D = true` pour absolument tout le monde. L'axe Z est verrouillé à 0.0f pour le gameplay (seul le visuel peut utiliser la profondeur).
- **Tower Defense** : Les tourelles détecteront les corps rigides (météores/pirates) via un simple `RadiusQuery` ou `Raycast` 2D sur le plan de jeu.
- **Smart Camera** : Le composant `SmartCamera` actuel sera étendu pour s'attacher dynamiquement (via l'`EventBus`) à n'importe quel `Entity` (Vaisseau, Planète A, Planète B).
- **Événements FTL** : Création d'un `EventManager` qui tire au sort (RNG) des désastres ou des rencontres aléatoires basées sur un timer ou la position du joueur dans l'espace.
