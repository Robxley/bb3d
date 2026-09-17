# Plan d'implémentation du Système d'Ombres (CSM + PCF)

**Objectif :** Implémenter un système d'ombres directionnelles en temps réel utilisant les Cascaded Shadow Maps (CSM) et le Percentage-Closer Filtering (PCF) pour des ombres douces.

**Architecture :** Le système de rendu (Renderer) calculera les matrices de vue-projection pour 4 cascades depuis le frustum de la caméra active. Il générera un Depth Framebuffer (Texture2DArray), liera ce buffer au pipeline global PBR/Toon, et effectuera l'échantillonnage PCF via les shaders.

**Stack Technique :** Vulkan 1.3 (Vulkan-Hpp), GLM, bb3d Engine Architecture, GLSL.

---

### Tâche 1: Mise à jour de la configuration (Config & Component)

**Fichiers concernés :**
- Modifier : `c:/dev/bb3d/include/bb3d/core/Config.hpp`
- Modifier : `c:/dev/bb3d/src/bb3d/core/Config.cpp`
- Modifier : `c:/dev/bb3d/include/bb3d/scene/Components.hpp`

**Étape 1 : Écrire le test qui échoue**
```cpp
// Dans unit_test_shadows.cpp (nouveau)
#include "bb3d/core/Config.hpp"
#include <cassert>

void test_config_shadows() {
    bb3d::EngineConfig cfg;
    assert(cfg.shadowsEnabled == true);
    assert(cfg.shadowCascades == 4);
    assert(cfg.shadowMapResolution == 2048);
    assert(cfg.shadowPCF == true);
}
```

**Étape 2 : Lancer le test pour vérifier l'échec**
Commande : `cmake --build build --target unit_test_shadows && .\build\bin\unit_test_shadows.exe`
Attendu : Échec car les propriétés n'existent pas dans `Config.hpp`.

**Étape 3 : Implémentation Minimale**
```cpp
// Dans Config.hpp, ajouter:
bool shadowsEnabled = true;
uint32_t shadowMapResolution = 2048;
uint32_t shadowCascades = 4;
bool shadowPCF = true;

// Dans Config.cpp, gérer `j.contains("shadow...")`
```

**Étape 4 : Lancer le test pour vérifier le succès**
Commande : `cmake --build build --target unit_test_shadows && .\build\bin\unit_test_shadows.exe`
Attendu : Succès (PASS).

**Étape 5 : Commit**
```bash
git add include/bb3d/core/Config.hpp src/bb3d/core/Config.cpp tests/unit_test_shadows.cpp
git commit -m "feat(core): ajout des parametres de configuration pour les ombres CSM"
```

---

### Tâche 2: Implémentation du Shadow Render Pass (Texture2DArray Depth)

**Fichiers concernés :**
- Modifier : `c:/dev/bb3d/include/bb3d/render/Renderer.hpp`
- Modifier : `c:/dev/bb3d/src/bb3d/render/Renderer.cpp`

**Étape 1 : Écrire le test qui échoue**
```cpp
// Dans le test unitaire
auto engine = bb3d::Engine::Create(...);
// Assert qu'un m_shadowRenderPass ou getShadowPass existe et retourne vk::RenderPass valide.
```

**Étape 2 : Lancer le test pour vérifier l'échec**

**Étape 3 : Implémentation Minimale**
```cpp
// Créer un vk::Image depth avec layerCount = shadowCascades.
// Créer une Render Pass ne gardant que l'attachement profondeur sans couleur.
```

**Étape 4 : Lancer le test**

**Étape 5 : Commit**
```bash
git commit -m "feat(render): creation framebuffers multi-layers pour shadow map"
```

---

### Tâche 3: Calcul Mathématique des Cascades (CPU)

**Fichiers concernés :**
- Créer : `c:/dev/bb3d/include/bb3d/render/ShadowCascade.hpp`
- Créer : `c:/dev/bb3d/src/bb3d/render/ShadowCascade.cpp`

**Étape 1 : Écrire un test de mathématiques**
```cpp
// Tester la découpe d'un frustum en N parties et valider les split distances (ex: Logarithmic split).
```

**Étape 2 : Lancer le test (Échec)**

**Étape 3 : Implémentation (Logarithmic Split & Light Matrices)**
```cpp
// Extraire les 8 coins du sous-frustum.
// Aligner une matrice Orthographique englobante depuis la perspective de la lumière.
```

**Étape 4 : Lancer le test (Succès)**

**Étape 5 : Commit**
```bash
git commit -m "feat(render): algorithme de decoupe du view frustum pour CSM"
```

---

### Tâche 4: Shaders et Pipelines

**Fichiers concernés :**
- Créer : `c:/dev/bb3d/assets/shaders/shadow.vert`
- Modifier : `c:/dev/bb3d/assets/shaders/pbr.frag`
- Modifier : `c:/dev/bb3d/assets/shaders/toon.frag`
- Modifier : `Renderer.cpp` (lister `shadow.vert` / descripteurs)

**Étape 1 : Lancer le compilateur SPIR-V via les scripts de build pour vérifier l'échec d'inclusion**

**Étape 2 : Écrire les shaders**
```glsl
// shadow.vert : multiplier position par cascadeVP[cascadeIndex]
// pbr.frag : ajouter sampler2DArrayShadow et calcul de PCF (Percentage Closer Filtering)
```

**Étape 3 : Compiler les shaders**
```bash
# S'assure de l'intégrité GLSL (build/shaders)
cmake --build build
```

**Étape 4 : Commit**
```bash
git commit -m "feat(shader): GLSL pour le rendu et la lecture CSM/PCF"
```

---

### Tâche 5: Soumission Multi-Passes (Renderer Logic)

**Fichiers concernés :**
- Modifier : `Renderer::render()` et `drawScene()`

**Étape 1 : Activer le rendu Offscreen dans le code**
```cpp
// Boucler de 0 à shadowCascades-1 dans le RenderPass d'ombre.
// Exécuter Draw().
// Lier la DepthArray generée dans le descripteur Global de la passe Forward.
```

**Étape 2 : Test final intégré**
Lancer `unit_test_shadows` avec rendu visuel, faire spawner des objets sur le soleil pour valider la génération des ombres.

**Étape 3 : Commit**
```bash
git commit -m "feat(render): Integration pipeline Shadow Pass & Forward Pass"
```
