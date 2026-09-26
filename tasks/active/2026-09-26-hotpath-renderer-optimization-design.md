# Conception Technique : Optimisation Hot-Path Renderer (Points 1, 2 et 3)

- **Date :** 2026-09-26
- **Auteur :** Antigravity
- **Statut :** APPROUVÉ (Approche 3 validée par l'utilisateur)
- **Objectif :** Réduire la latence CPU du hot-path de rendu ([`Renderer::prepareRenderData()`](file:///c:/dev/bb3d/src/bb3d/render/Renderer.cpp#L938)), optimiser le tri de commandes ([`RenderCommand`](file:///c:/dev/bb3d/include/bb3d/render/Renderer.hpp#L28)), intégrer le Frustum Culling en amont, et vectoriser le streaming vers l'Instance Buffer selon la Règle Suprême 0 (Performance Temps Réel Moteur 3D).

---

## 1. Contexte & Problématique

Dans l'implémentation actuelle de `Renderer` :
1. **Tri coûteux en bande passante L1/L2 :** [`RenderCommand`](file:///c:/dev/bb3d/include/bb3d/render/Renderer.hpp#L28) occupe **96 octets** (dont une matrice `glm::mat4` de 64 octets). Lors du `std::ranges::sort()`, chaque permutation (`std::swap`) déplace 96 octets en mémoire, alors que la comparaison ne teste que `(type, material, mesh)` (~20 octets).
2. **Double travail sur le Frustum Culling :** Toutes les entités sont d'abord ajoutées à `m_renderCommands` en calculant leur transformation matricielle complète, puis un second passage `std::remove_if` recalcule une boîte AABB transformée (8 multiplications matricielles complètes) et fait un `m_renderCommands.erase()`, provoquant des décalages mémoire de 96 octets par élément.
3. **Copie discontinue matrice par matrice :** Le transfert des matrices vers le buffer GPU mappé d'instances `mappedData` s'effectue via une boucle `memcpy` élément par élément sautant par pas de 96 octets.

---

## 2. Architecture Technique Retenue (Approche 3)

### 2.1. Compactage de `RenderCommand` (96B ➔ 24B)
La matrice `glm::mat4 transform` est extraite de `RenderCommand` et stockée dans un vecteur réservé `std::vector<glm::mat4> m_transforms`.
`RenderCommand` ne conserve qu'un index 32-bit vers ce vecteur :

```cpp
struct RenderCommand {
    MaterialType type;        // 4 octets
    bool castShadows;         // 1 octet
    uint8_t padding[3];       // 3 octets (alignement 32-bit)
    uint32_t transformIndex;  // 4 octets
    Material* material;       // 8 octets
    Mesh* mesh;               // 8 octets

    inline bool operator<(const RenderCommand& other) const noexcept {
        if (type != other.type) return type < other.type;
        if (material != other.material) return material < other.material;
        return mesh < other.mesh;
    }
}; // Total = 24 octets (au lieu de 96 octets) ➔ -75% de mémoire déplacée lors du tri !
```

### 2.2. Frustum Culling en Amont (Pré-Insertion)
Au lieu d'insérer aveuglément puis de filtrer via `remove_if` :
1. Lors du parcours des entités (`MeshComponent`, `ModelComponent`) :
   - On calcule le transform de l'entité.
   - On calcule l'AABB monde `worldBounds = mesh->getBounds().transform(transform)`.
   - On teste la visibilité :
     ```cpp
     bool inFrustum = !m_config.graphics.enableFrustumCulling || m_frustum.intersects(worldBounds);
     bool shadowCasterInRange = castShadows && m_shadowsEnabledRuntime && (distSq <= shadowFarZSq);
     if (!inFrustum && !shadowCasterInRange) {
         continue; // CULLED : Zéro push_back, zéro allocation, zéro décalage mémoire ultérieur !
     }
     ```
2. Les commandes rejetées ne polluent plus `m_renderCommands`.
3. Le bloc `std::remove_if` et `m_renderCommands.erase()` est totalement éliminé.

### 2.3. Remplissage Optimisé de l'Instance Buffer GPU
Après le tri ultra-rapide des 24 octets de `RenderCommand` :
- Le buffer mappé (`mappedData`) est écrit séquentiellement :
  ```cpp
  void* mappedData = m_instanceBuffers[m_currentFrame]->getMappedData();
  uint32_t count = (std::min)((uint32_t)m_renderCommands.size(), MAX_INSTANCES);
  auto* dst = static_cast<glm::mat4*>(mappedData);
  for (uint32_t i = 0; i < count; ++i) {
      dst[i] = m_transforms[m_renderCommands[i].transformIndex];
  }
  ```
- L'écriture dans `dst[i]` est 100% contiguë (streaming store optimal vers la VRAM hôte).
- La lecture de `m_renderCommands[i].transformIndex` bénéficie de la densité quadruple du vecteur en cache L1.

---

## 3. Impact & Compatibilité

* **API Publique & Shaders :** 0 modification de l'API publique (`Renderer.hpp`, `Scene.hpp`, etc.) et 0 modification des shaders GLSL.
* **Passes de rendu :**
  - `drawScene()` : continue d'utiliser `cmd.type`, `cmd.material`, `cmd.mesh` et les indices d'instances contigus `currentBatchStart`.
  - `renderShadows()` : continue d'utiliser `cmd.castShadows`, `cmd.mesh` et `batchStart = cmdIdx`.
* **Tests & Validation :**
  - Nouveau test unitaire TDD : `tests/unit_test_36_renderer_hotpath_optim.cpp`.
  - Vérification de la non-régression de l'ensemble de la suite de tests CTest.
