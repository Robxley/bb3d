# Design Document: Système de Particules Hybride (bb3d)

## 1. Objectif
Ajouter un système d'émission de particules natif au moteur `bb3d`. Ce système doit permettre des effets visuels (feu, fumée) très performants tout en supportant occasionnellement des particules avec collisions rigides (physique Jolt). Le premier cas d'usage sera le propulseur de la fusée `AstroBazard`.

## 2. Architecture des Composants

### 2.1 L'état des particules (`bb3d::Particle`)
Une structure C++ simple représentant les données dynamiques d'une seule particule.
- `glm::vec3 position`, `glm::vec3 velocity`
- `glm::vec4 colorBegin`, `glm::vec4 colorEnd`
- `float sizeBegin`, `float sizeEnd`
- `float lifeTime`, `float lifeRemaining`
- Optionnel : Handle d'entité physique (`bb3d::Entity`) si la particule est activée dans Jolt.

### 2.2 Le Gestionnaire / Composant (`bb3d::ParticleSystemComponent`)
Attachable à n'importe quelle entité (ex: le tuyère du vaisseau).
- Contient un pool pré-alloué de `Particle` (ex: 1000 max).
- Gère une file d'émission (Emit(const ParticleProps& props)).
- Lors du rendu, génère un VBO dynamique ou utilise l'instanciation SSBO (comme testé dans Hello World) pour l'affichage rapide.
- **Paramètres globaux** : `emitRate`, `duration`, `visualType` (Billboard2D ou Mesh3D).
- **Physique** : Un flag booléen `injectIntoPhysics`. Si vrai, chaque émission crée un `PhysicsComponent(Dynamic)` associé, sinon c'est purement cinématique C++.

## 3. Implémentation Visuelle (Vulkan)
Puisque le moteur ne supporte pas encore nativement les Geometry Shaders (souvent obsolètes) et pour rester simple et compatible :
- Le rendu des particules 2D se fera via l'instanciation GPU (SSBO). On instancie un simple `Quad` face à la caméra pour simuler le Billboard.
- Le rendu des particules 3D passera par l'instanciation GPU d'un Mesh fourni (ex: Cube).
- Cela s'aligne exactement sur le standard décrit dans `GEMINI.md` (Instancing obligatoire pour les particules).

## 4. Intégration AstroBazard
- Les scripts `SpaceshipController.cpp` instancieront nativement un `ParticleSystemComponent` sous le nez de la fusée.
- La touche `Espace` (ou `Z`) appellera `emit()` avec une vélocité orientée vers le bas.
- Les touches `Q/A` et `D` déclencheront des mini-propulseurs latéraux.
