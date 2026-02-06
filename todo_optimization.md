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
*   **Statut :** Terminé. Les allocations dynamiques dans le "Hot Path" de rendu sont minimisées.

### 2. Gestion Dynamique des Descripteurs
*   **Fichier :** `src/bb3d/render/Renderer.cpp` (Méthode `createGlobalDescriptors`)
*   **Problème :** Le `DescriptorPool` global a une taille fixe (2000 sets, hardcodé).
*   **Action :**
    *   [ ] Créer un `DescriptorAllocator` capable de gérer plusieurs pools.
    *   [ ] Si un pool est plein, en créer un nouveau automatiquement.
    *   [ ] Reset les pools inutilisés pour éviter la fragmentation.
*   **Statut :** À faire. Risque de crash sur les grosses scènes.

### 3. Réduction de l'empreinte RAM des Meshs
*   **Fichier :** `include/bb3d/render/Mesh.hpp`
*   **Problème :** La classe `Mesh` possède déjà la méthode `releaseCPUData()`, mais elle doit être appelée manuellement.
*   **Action :**
    *   [ ] Appeler automatiquement `releaseCPUData()` après l'upload GPU pour les meshes statiques (via un flag dans `Model` ou `Mesh`).
    *   [ ] Vérifier que cela n'impacte pas les `MeshCollider` de Jolt Physics (qui ont besoin des vertices CPU).
*   **Statut :** Fonctionnalité existante, intégration automatique à faire.

## 🟡 Priorité : Moyenne (Gain CPU, Effort Modéré)

### 4. Vrais Uploads Asynchrones (Correction)
*   **Fichier :** `src/bb3d/render/Buffer.cpp`, `src/bb3d/render/VulkanContext.cpp`
*   **Problème :** `Buffer::CreateVertexBuffer` utilise `context.endSingleTimeCommands()` qui appelle **`m_graphicsQueue.waitIdle()`**. Cela bloque le CPU à chaque création de buffer.
*   **Action :** 
    *   [x] Créer un `StagingManager` (Fait).
    *   [ ] **Remplacer** `waitIdle` par l'utilisation d'une `vk::Fence` dédiée au transfert.
    *   [ ] Implémenter une `TransferQueue` dédiée (si disponible sur le GPU) pour ne pas bloquer la Graphics Queue.
*   **Statut :** À faire. Bloquant pour le streaming d'assets.

### 5. Parallélisation du Culling & Tri
*   **Fichier :** `src/bb3d/render/Renderer.cpp`
*   **Problème :** Le culling Frustum et le tri des commandes (`std::sort`) sont faits sur le thread principal dans `drawScene`.
*   **Action :** 
    *   [ ] Utiliser `JobSystem::dispatch` pour paralléliser le test d'intersection AABB/Frustum.
    *   [ ] Utiliser `std::execution::par` avec `std::sort` (si supporté par le compilateur) ou un tri par blocs.
*   **Statut :** À faire. Le Culling est actuellement séquentiel.

### 6. Optimisation Chargement Modèles
*   **Fichier :** `src/bb3d/render/Model.cpp`
*   **Problème :** `tinyobjloader` est lent (parsing texte). Le chargement des textures dans `loadOBJ` est synchrone et séquentiel.
*   **Action :**
    *   [ ] Privilégier GLTF/GLB (`fastgltf` déjà intégré).
    *   [ ] Charger les textures en parallèle via `JobSystem` lors du chargement du modèle.
*   **Statut :** À faire.

## 🔴 Priorité : Basse / Recherche (Expertise requise)

### 7. Affinage des barrières de synchronisation Vulkan
*   **Fichier :** `src/bb3d/render/Renderer.cpp` (Méthode `compositeToSwapchain`)
*   **Problème :** Usage de `vk::PipelineStageFlagBits::eTopOfPipe` et `vk::PipelineStageFlagBits::eBottomOfPipe`.
*   **Action :** 
    *   [ ] Remplacer par des stages précis (ex: `COLOR_ATTACHMENT_OUTPUT` -> `FRAGMENT_SHADER`).
    *   [ ] Utiliser `vk::DependencyInfo` (Vulkan 1.3) pour simplifier la syntaxe.
*   **Statut :** À faire. Optimisation GPU (réduction des bulles).

### 8. GPU-Driven Rendering
*   **Problème :** Trop de draw calls côté CPU.
*   **Action :** 
    *   [ ] Implémenter `vkCmdDrawIndexedIndirect`.
    *   [ ] Culling GPU (Compute Shader).
*   **Statut :** Recherche.

---
*Dernière mise à jour : 06 Février 2026 (Revue de Code)*