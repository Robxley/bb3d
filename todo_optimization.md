# 🚀 Optimisations Performance - biobazard3d

Ce document liste les optimisations identifiées lors de la revue de code du moteur (Février 2026).

## 🟢 Priorité : Haute (Gain immédiat, Faible effort)

### 1. Élimination des allocations par frame (Heap Pressure)
*   **Fichier :** `src/bb3d/render/Renderer.cpp`
*   **Problème :** Les vecteurs `commands` et `instanceTransforms` dans `drawScene` sont réalloués à chaque frame.
*   **Action :** 
    *   [x] Déplacer les vecteurs en membres privés de la classe `Renderer`.
    *   [x] Utiliser `reserve()` au démarrage.
    *   [x] Appeler `clear()` à chaque début de frame au lieu de détruire l'objet.
*   **Bénéfice :** Zéro allocation dynamique dans le "Hot Path" de rendu. (Terminé : Février 2026)

### 2. Gestion Dynamique des Descripteurs
*   **Fichier :** `src/bb3d/render/Material.cpp` / `Renderer.cpp`
*   **Problème :** Le `DescriptorPool` global a une taille fixe (2000 sets). Chaque nouveau matériel alloue un set via `vk::Device::allocateDescriptorSets` sans vérifier la capacité.
*   **Action :**
    *   [ ] Créer un `DescriptorAllocator` capable de gérer plusieurs pools.
    *   [ ] Si un pool est plein, en créer un nouveau automatiquement.
    *   [ ] Reset les pools inutilisés pour éviter la fragmentation.
*   **Bénéfice :** Évite le crash de l'application sur les scènes avec beaucoup de matériaux (>2000).

### 3. Réduction de l'empreinte RAM des Meshs
*   **Fichier :** `include/bb3d/render/Mesh.hpp`
*   **Problème :** La classe `Mesh` conserve `std::vector<Vertex> m_vertices` et `m_indices` en RAM même après l'upload sur le GPU.
*   **Action :**
    *   [ ] Ajouter une option ou une méthode `releaseCPUData()` pour libérer ces vecteurs après la création des buffers GPU.
*   **Bénéfice :** Réduction significative de la consommation mémoire (RAM) pour les modèles complexes.

## 🟡 Priorité : Moyenne (Gain CPU, Effort Modéré)

### 4. Vrais Uploads Asynchrones (Correction)
*   **Fichier :** `src/bb3d/render/Buffer.cpp`
*   **Problème :** Bien qu'un `StagingBuffer` soit alloué, `Buffer::CreateVertexBuffer` utilise `context.endSingleTimeCommands()` qui appelle **`m_graphicsQueue.waitIdle()`**. Cela bloque le CPU à chaque création de buffer (Mesh/Texture).
*   **Action :** 
    *   [x] Créer un `StagingManager` (Fait).
    *   [ ] **Remplacer** `waitIdle` par l'utilisation d'une `vk::Fence` dédiée au transfert.
    *   [ ] Ou utiliser un `TransferQueue` dédié si disponible.
*   **Bénéfice :** Suppression des blocages CPU lors du chargement de scène (100 meshes = 100 attentes GPU).

### 5. Parallélisation du Culling & Tri
*   **Fichier :** `src/bb3d/render/Renderer.cpp`
*   **Problème :** Le culling Frustum et le tri des commandes sont faits sur un seul thread.
*   **Action :** 
    *   [x] Utiliser `JobSystem::dispatch` pour paralléliser le test d'intersection AABB/Frustum.
    *   [ ] Utiliser `std::sort` parallèle si possible ou trier par blocs.
*   **Bénéfice :** Meilleure utilisation des processeurs multi-cœurs sur les scènes chargées. (Partiellement Fait)

### 6. Optimisation TinyObjLoader
*   **Fichier :** `src/bb3d/render/Model.cpp`
*   **Problème :** `tinyobjloader` est lent pour les gros fichiers OBJ (parsing texte).
*   **Action :**
    *   [ ] Privilégier GLTF/GLB (déjà supporté via `fastgltf`).
    *   [ ] Convertir les assets OBJ en GLB lors de l'import ou utiliser un parser binaire custom.
*   **Bénéfice :** Réduction drastique du temps de chargement des modèles.

## 🔴 Priorité : Basse / Recherche (Expertise requise)

### 7. Affinage des barrières de synchronisation Vulkan
*   **Fichier :** `src/bb3d/render/Renderer.cpp` / `SwapChain.cpp`
*   **Problème :** Usage fréquent de `TOP_OF_PIPE` et `BOTTOM_OF_PIPE`.
*   **Action :** 
    *   [ ] Remplacer par des stages précis (ex: `COLOR_ATTACHMENT_OUTPUT` -> `FRAGMENT_SHADER`).
    *   [ ] Utiliser `vk::DependencyInfo` (Vulkan 1.3) pour simplifier la syntaxe.
*   **Bénéfice :** Réduction des temps d'attente "bulles" du GPU.

### 8. GPU-Driven Rendering (Indirect Draw & GPU Culling)
*   **Problème :** Trop de draw calls côté CPU et culling CPU limitant pour les scènes massives.
*   **Action :** 
    *   [ ] Implémenter un SSBO pour les matrices d'objets.
    *   [ ] Utiliser `vkCmdDrawIndexedIndirect` pour envoyer les commandes de dessin groupées.
    *   [ ] Implémenter un Compute Shader pour le culling Frustum.
*   **Bénéfice :** Décharge totale du CPU pour la visibilité et la soumission des commandes de dessin.

---
*Note : Document mis à jour par l'agent Gemini CLI le 06 Février 2026.*
