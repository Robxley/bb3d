# Design Document: Cascaded Shadow Maps (CSM) avec option PCF

## Objectif
Ajouter un système d'ombrage directionnel professionnel, performant et évolutif au moteur `biobazard3d`. L'ombrage principal (Lumière directionnelle/Soleil) utilisera la technique des **Cascaded Shadow Maps (CSM)** pour pallier le problème de crénelage de perspective, avec un filtrage **PCF (Percentage-Closer Filtering)** optionnel activable dans la configuration matérielle.

## Architecture & Concepts

### 1. Composant & Configuration (Core)
- **`LightComponent`** : Utilisation de la variable booléenne existante `castShadows`.
- **`Config` (JSON)** :
  - `shadowsEnabled` (bool)
  - `shadowMapResolution` (int, ex: 2048)
  - `shadowCascades` (int, par defaut: 4, max: 4)
  - `shadowPCF` (bool, active le filtrage doux).

### 2. Gestion Vulkan (Backend Renderer)
- **Render Target (Texture Array)** : Une seule et unique `vk::Image` avec un `layerCount` égal au nombre de cascades (ex: 4). La vue sera de type `Texture2DArray` (Depth format : `D32_SFLOAT`).
- **Render Pass & Framebuffer** : Une nouvelle Passe de rendu `m_shadowRenderPass` qui génère uniquement le Framebuffer de profondeur multi-couches.
- **Pipeline Graphique PBR/Toon** : Le descripteur `GlobalUBO` va maintenant lier la Shadow Map et inclure un nouveau Uniform Buffer contenant les matrices de Light View-Projection pour chaque cascade ainsi que les distances de fractionnement (split distances).

### 3. Logique CPU (Calcul des Cascades)
Dans `Renderer::drawScene` (ou une fonction dédiée `Renderer::updateShadows`) :
- Récupérer le **View Frustum** de la caméra active.
- Diviser ce frustum en `N` sous-frustums selon l'axe Z (stratégie logarithmique/pratique, ex: $P_i = C_{near} \times \left(\frac{C_{far}}{C_{near}}\right)^{\frac{i}{N}}$).
- Pour chaque sous-frustum, calculer la Bounding Sphere ou le Bounding Box, puis l'orienter selon la direction de la lumière pour construire une matrice Orthographique (Projection) et une matrice de Vue (View) englobant précisément cette région.

### 4. Shaders (GLSL)
- **`shadow.vert`** : Un shader très simple reprenant le `VertexLayout` standard. `gl_Position = cascadeVP[gl_InstanceIndex] * ModelData * vec4(inPos, 1.0)`. Note : L'Instance Index ou Push Constant peut guider la cascade courante si on boucle les draw calls.
- **`pbr.frag` / `toon.frag`** : 
  - Comparer `gl_FragCoord.z` avec les `splitDistances` pour déterminer l'index de cascade.
  - Calculer la coordonnée fantôme de la lumière et sélectionner la couche `z` de la `sampler2DArrayShadow`.
  - Si `PCF` est activé (via Spécialisation Constant Vulkan ou `#define`), prélever 4 à 9 échantillons voisins autour du pixel avec des offsets interpolés pour lisser la bordure de l'ombre de façon dynamique.
  
## Séquencement d'exécution
1. **Pass Ombre (Offscreen)**
   - Lier `m_shadowRenderPass`.
   - Pour chaque cascade (0 à 3) :
     - Assigner la matrice LightVP de la cascade (Push Constant).
     - Parcourir les `RenderCommand` qui castent des ombres (Opaque).
     - Dessiner dans le layer correspondant du Framebuffer (via viewport/scissor par layer ou Geometry Shader instancing, nous privilégierons des drawcalls séparés par layers pour la simplicité Vulkan initialement : multiview ou attachement par layer).
2. **Pass Principale (Forward)**
   - Lier la color/depth standard.
   - Les shaders color fragment utiliseront la cascade générée.

## Avantages
- L'utilisation de `Texture2DArray` réduit dramatiquement les bindings par rapport au rendu de multiples textures pour différentes cascades.
- Le paramétrage via JSON de la config permet aux développeurs de scale du mobile (1 cascade, 1024px) jusqu'au PC (4 cascades PCF, 4096px) en respectant les standards architecturaux imposés dans la doctrine.
