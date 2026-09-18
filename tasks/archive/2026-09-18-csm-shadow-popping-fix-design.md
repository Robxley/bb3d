# Document de Conception : Correction du Popping des Ombres en Cascade (CSM)

- **Date :** 2026-09-18
- **Auteur :** @Antigravity
- **Sujet :** Disparition et réapparition brutale des ombres selon la position/orientation de la caméra
- **Fichiers ciblés :**
  - `src/bb3d/render/ShadowCascade.cpp`
  - `src/bb3d/render/Renderer.cpp`
  - `assets/shaders/pbr.frag`
  - `assets/shaders/toon.frag`
  - `tests/unit_test_shadows.cpp`

---

## 1. Contexte & Analyse Critique de l'Anomalie

L'utilisateur a signalé une anomalie visuelle où les ombres directionnelles (CSM) apparaissent et disparaissent brutalement en fonction de la position et de l'orientation de la caméra.

Après inspection critique et exhaustive du code source, **trois causes racines combinées** ont été confirmées :

### Cause 1 : Troncature Asymétrique du Frustum dans `ShadowCascade.cpp`
Dans `ShadowCascade::calculateLightSpaceMatrix` (lignes 28 à 57) :
```cpp
for (unsigned int x = 0; x < 2; ++x) {
    for (unsigned int y = 0; y < 2; ++y) {
        for (unsigned int z = 0; z < 2; ++z) {
            glm::vec4 pt = invProj * glm::vec4(2.0f * x - 1.0f, 2.0f * y - 1.0f, z, 1.0f);
            viewCorners.push_back(pt / pt.w);
        }
    }
}
// ...
for (int i = 0; i < 4; ++i) {
    glm::vec3 dir = glm::normalize(glm::vec3(viewCorners[i + 4])); 
    glm::vec3 wNear = glm::vec3(invView * glm::vec4(dir * nearZ, 1.0f));
    glm::vec3 wFar = glm::vec3(invView * glm::vec4(dir * farZ, 1.0f));
    // ...
}
```
- `viewCorners` génère 8 sommets NDC. Les indices 0 à 3 correspondent à $x = 0$ (côté gauche de l'écran, $x = -1$). Les indices 4 à 7 correspondent à $x = 1$ (côté droit de l'écran, $x = +1$).
- La boucle `for (int i = 0; i < 4; ++i)` lit `viewCorners[i + 4]`, soit **uniquement la moitié droite de l'écran** ! La moitié gauche est totalement ignorée.
- De plus, les points sont normalisés puis multipliés par `nearZ`/`farZ` sans tenir compte de la profondeur le long de l'axe optique.
- **Résultat :** La sphère englobante de la cascade est fortement décalée sur la droite. Dès que la caméra tourne ou se déplace, les objets situés au centre ou à gauche sortent du volume de la cascade et leurs ombres disparaissent net.

### Cause 2 : Élimination Prématurée des Projeteurs d'Ombres par le Frustum Culling Caméra dans `Renderer.cpp`
Dans `Renderer::prepareRenderData` (lignes 983 à 990) :
```cpp
if (m_config.graphics.enableFrustumCulling && !m_renderCommands.empty()) {
    auto it = std::remove_if(m_renderCommands.begin(), m_renderCommands.end(),
        [&](const RenderCommand& cmd) {
            if (!cmd.mesh) return false;
            AABB worldBounds = cmd.mesh->getBounds().transform(cmd.transform);
            return !m_frustum.intersects(worldBounds);
        });
    m_renderCommands.erase(it, m_renderCommands.end());
}
```
- Tout objet qui n'intersecte pas `m_frustum` (le champ de vision de la caméra) est définitivement purgé de `m_renderCommands`.
- Or, `renderShadows()` itère sur ce même `m_renderCommands` pour dessiner les shadow maps.
- **Résultat :** Un objet volumineux ou haut (bâtiment, arbre, astéroïde) situé hors du champ caméra (juste au-dessus, derrière ou sur le côté) mais dont l'ombre est projetée dans le champ de vision est supprimé. Dès qu'il quitte le champ de la caméra, son ombre au sol s'éteint instantanément.

### Cause 3 : Indexation et Sélection de Cascade Erronée dans les Shaders (`pbr.frag` et `toon.frag`)
Dans `pbr.frag` et `toon.frag` :
```glsl
int layer = 0;
for(int i = 1; i < 4; ++i) {
    if(depth <= ubo.shadowSplitDepths[i]) {
        layer = i;
        break;
    }
}
```
- La boucle démarre à `i = 1`.
- Si `depth <= ubo.shadowSplitDepths[0]` (objet proche), la condition `depth <= ubo.shadowSplitDepths[1]` est vraie -> `layer = 1`. **La cascade 0 (la plus précise) n'est jamais sélectionnée pour les objets au premier plan.**
- Si `depth > ubo.shadowSplitDepths[3]`, aucun test n'est valide et `layer` reste à `0` (les objets très lointains échantillonnent la cascade de gros plan).

---

## 2. Spécification Technique de la Solution

### A. Calcul Géométrique Exact du Frustum (`ShadowCascade.cpp`)
Pour chaque cascade [nearZ, farZ] :
1. Unprojeter les 4 rayons de coins NDC (x dans {-1, 1}, y dans {-1, 1}) à travers `invProj`.
2. Calculer pour chaque coin le vecteur direction dans l'espace vue :
   d = v / abs(v.z) où v = invProj * vec4(x, y, 1.0, 1.0).
3. Générer les 4 sommets proches v_near = d * (-nearZ) et les 4 sommets lointains v_far = d * (-farZ) dans l'espace vue (où la caméra regarde vers -Z).
4. Transformer les 8 sommets en espace monde via `invView`.
5. Calculer le centre exact : center = (1/8) sum(w_k).
6. Calculer le rayon de la sphère englobante : radius = max(norm(w_k - center)).
7. Positionner la caméra de lumière avec une marge arrière adéquate pour englober les casters en amont :
   eye = center - lightDir * (radius * 4.0f + 50.0f)
   et projeter en orthographique symétrique [-radius, radius] avec texel snapping.

### B. Préservation des Projeteurs d'Ombres dans `Renderer.cpp`
Dans `prepareRenderData` :
- Le frustum culling caméra ne doit pas purger les objets ayant `cmd.castShadows == true` s'ils se trouvent dans le rayon d'influence des ombres (`distance2(camPos, objectCenter) <= shadowFarZ * shadowFarZ`).
- Les objets hors du champ caméra qui projettent des ombres sont conservés dans `m_renderCommands` pour `renderShadows()`.
- Lors du rendu de la scène principale (`drawScene`), les polygones hors du champ caméra sont automatiquement éliminés par le clipping matériel Vulkan sans surcoût de rasterisation.

### C. Correction de la Sélection de Cascade (`pbr.frag`, `toon.frag`)
Remplacer la boucle de sélection de cascade par :
```glsl
int layer = -1;
for (int i = 0; i < 4; ++i) {
    if (depth <= ubo.shadowSplitDepths[i]) {
        layer = i;
        break;
    }
}
if (layer < 0) {
    return 1.0; // En dehors de toutes les cascades : pleinement éclairé, aucune ombre
}
```

---

## 3. Plan de Test & Validation TDD

1. **Test Mathématique (`unit_test_shadows.cpp`) :**
   - Vérifier que les 8 coins du sub-frustum (y compris x = -1 et x = +1) projettent tous strictement dans le cube [-1, 1] x [-1, 1] x [0, 1] de la matrice de lumière `lightVP`.
   - Vérifier que le centre et le rayon de la sphère englobante sont isotropes (invariants par rotation de la caméra autour de l'axe optique).
2. **Compilation des Shaders :**
   - Vérifier la recompilation SPV sans avertissement via `glslc`.
3. **Tests de Non-Régression :**
   - 100% de passage de la suite CTest.
