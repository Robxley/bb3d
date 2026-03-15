# Plan d'implémentation : Système de Planètes Procédurales

**Objectif :** Créer un système de génération de planètes basé sur une Cube Sphere avec biomes multiples (Climat), relief varié et scattering d'objets.

**Architecture :** 
- Topologie **Normalized Cube Sphere** (6 faces planes projetées sur une sphère).
- Générateur de bruit **GLM Noise** utilisé dans le `JobSystem`.
- Système de **Climate Map** (Humidité/Température) pour le choix des biomes.
- Intégration Jolt via `MeshShape` pour les collisions.

**Stack Technique :** C++20, GLM, Jolt Physics, Vulkan-Hpp, EnTT.

---

### Tâche 1 : Intégration & Test du Bruit (GLM)
**Fichiers concernés :** 
- Créer : `tests/unit_test_noise.cpp`
- Modifier : `CMakeLists.txt`

**Étape 1 : Écrire le test qui échoue**
Vérifier si `glm::perlin` ou `glm::simplex` est disponible et produit des valeurs cohérentes.
```cpp
#include <glm/gtc/noise.hpp>
#include <iostream>
int main() {
    float n = glm::perlin(glm::vec3(0.5f));
    std::cout << "Noise: " << n << std::endl;
    return 0;
}
```

**Étape 2 : Lancer le test**
Commande : `cmake --build build && ctest -R unit_test_noise`

**Étape 3 : Implémentation Minimale**
S'assurer que `#include <glm/gtc/noise.hpp>` est inclus dans les futurs générateurs.

---

### Tâche 2 : Structure ProceduralPlanetComponent
**Fichiers concernés :** 
- Modifier : `include/bb3d/scene/Components.hpp`
- Modifier : `src/bb3d/scene/SceneSerializer.cpp`

**Étape 1 : Définir les structures de données**
Ajouter `BiomeSettings` et `ProceduralPlanetComponent`.
```cpp
struct BiomeSettings {
    std::string name;
    glm::vec3 color;
    float frequency;
    float amplitude;
    // ... params pour le type de bruit
};
```

---

### Tâche 3 : Générateur de Cube Sphere
**Fichiers concernés :** 
- Créer : `include/bb3d/render/ProceduralMeshGenerator.hpp`
- Créer : `src/bb3d/render/ProceduralMeshGenerator.cpp`
- Tester : `tests/unit_test_procedural_planet.cpp`

**Étape 1 : Génération des 6 faces**
Implémenter la logique qui subdivise chaque face d'un cube et normalise les sommets.
Le `JobSystem` doit être utilisé pour générer les 6 faces en parallèle.

---

### Tâche 4 : Système de Relief & Biomes (Climate Map)
**Fichiers concernés :** 
- Modifier : `src/bb3d/render/ProceduralMeshGenerator.cpp`

**Étape 1 : Déplacement des sommets**
Appliquer le bruit multi-octaves sur le rayon de chaque sommet.
Calculer la couleur du vertex en fonction du biome dominant (Climate Noise).

---

### Tâche 5 : Intégration Physique (Jolt)
**Fichiers concernés :** 
- Modifier : `src/bb3d/physics/PhysicsWorld.cpp`

**Étape 1 : Créer la MeshShape**
Adapter le `PhysicsWorld` pour qu'il puisse générer un `MeshShape` à partir d'un mesh procédural complexe.

---

### Tâche 6 : Scattering d'objets
**Fichiers concernés :** 
- Créer : `src/bb3d/scene/ScatteringSystem.cpp`

**Étape 1 : Placement instancié**
Parcourir le mesh généré et ajouter des instances de modèles (rochers/arbres) via le `Renderer`.

---

### Tâche 7 : Démo AstroBazard
**Fichiers concernés :** 
- Modifier : `apps/astro_bazard/main.cpp`

**Étape 1 : Configuration de la planète**
Remplacer la sphère statique par une planète procédurale riche.
