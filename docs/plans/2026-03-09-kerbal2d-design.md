# Concept : "Kerbal Space 2D" (Jeu de simulation spatiale 2D avec rendu 3D)

## 1. Vision Globale
Un jeu de simulation spatiale se déroulant sur un plan 2D (X/Y) mais exploitant l'aspect visuel du moteur 3D (modèles 3D, éclairage PBR, ombres, particules). Le joueur contrôle une fusée devant décoller d'une planète, gérer son orbite et atterrir.
La particularité visuelle réside dans l'effet de recul (zoom arrière de la caméra) progressif au fur et à mesure de l'ascension, révélant la courbure de la planète puis l'espace profond dans un effet continu (seamless).

## 2. Noms Proposés pour l'Application
1. **CosmicAscent** : Nom épique, met l'accent sur la montée et le voyage.
2. **Orbit2D** : Nom descriptif et simple.
3. **AstroBazard** : Pour faire un clin d'œil au nom du moteur *biobazard3d*.

*(Recommandation : AstroBazard, pour la cohérence avec le projet global).*

## 3. Mécaniques de Jeu (Gameplay)
*   **Contrôles (2D physique) :** 
    *   Le vaisseau se déplace uniquement sur l'axe X (Latéral) et Y (Vertical). L'axe Z est verrouillé à 0.
    *   Touches : Propulseur principal (Haut), rotation gauche/droite (physique torque).
*   **Physique Jolt 3D contrainte en 2D :**
    *   Utiliser des corps rigides `Dynamic` pour la fusée, en verrouillant le mouvement sur l'axe Z (Linear DOF) et la rotation sur les axes X et Y (Angular DOF).
    *   La gravité devient dynamique : La direction et la force de la gravité dépendent de la distance et de l'angle par rapport au centre de la planète.
*   **Caméra Dynamique :**
    *   La caméra suit le vaisseau.
    *   Plus l'altitude augmente, plus la caméra recule sur l'axe Z (FOV constant mais position Z variable) pour donner l'échelle planétaire.

## 4. Graphismes et Affichage
*   **Planète :** Un modèle 3D de sphère massive positionnée en `(0, -RayonPlanete, 0)`.
*   **Vaisseau :** Un modèle 3D stylisé (ex: Apollo ou fusée moderne) avec un `ParticleSystemComponent` orienté vers le bas pour les réacteurs.
*   **Espace (SkySphere) :** Un fond cosmique étoilé.
*   **Lumière :** Un `DirectionalLight` agissant comme le soleil, créant un éclairage dramatique avec des ombres dures sur la fusée et un terminateur visible sur la planète.

## 5. Réorganisation du Dossier `apps/`
*   Créer `apps/bb3d_editor/` et y déplacer `bb3d_editor.cpp` et `CMakeLists.txt` (ou modifier le CMake root).
*   Créer `apps/bb3d_hello_world/` et y déplacer `bb3d_hello_world.cpp`.
*   Créer `apps/astro_bazard/` (ou nom choisi) et y placer le nouveau `main.cpp`.
*   Ajuster `apps/CMakeLists.txt` pour cibler ces sous-dossiers.
