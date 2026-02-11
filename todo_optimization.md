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

*Dernière mise à jour : 11 Février 2026 (Revue de Code)*



## 🔵 Nouvelles Optimisations Identifiées (Revue Globale)



### 9. Paramètres de Matériaux et Frames in Flight

*   **Problème :** Les classes `Material` (`PBRMaterial`, etc.) possèdent un seul `m_paramBuffer` et un seul `m_set` partagés par toutes les frames. Si on modifie un paramètre (ex: couleur) alors qu'une frame précédente est encore en cours de rendu sur le GPU, cela crée une condition de course (Race Condition).

*   **Action :** 

    *   [ ] Décupler `m_paramBuffer` et `m_set` pour avoir une instance par frame (MAX_FRAMES_IN_FLIGHT).

    *   [ ] Alternativement, utiliser des Push Constants pour les petits paramètres (couleur, rugosité).



### 10. Initialisation Réactive de la Physique

*   **Fichier :** `src/bb3d/physics/PhysicsWorld.cpp`

*   **Problème :** `PhysicsWorld::update` parcourt toutes les entités avec un `RigidBodyComponent` à chaque frame pour détecter les nouveaux corps (ID == 0xFFFFFFFF).

*   **Action :** 

    *   [ ] Utiliser les `OnComponentAdded` observers d'EnTT pour créer le corps Jolt dès l'ajout du composant.

    *   [ ] Supprimer le scan séquentiel dans l'update.



### 11. Chargement Parallèle des Textures dans les Modèles

*   **Fichier :** `src/bb3d/render/Model.cpp`

*   **Problème :** Les textures des modèles GLTF et OBJ sont chargées de manière séquentielle et synchrone.

*   **Action :** 

    *   [ ] Utiliser `JobSystem::execute` pour charger chaque texture en parallèle.

    *   [ ] Utiliser des "Futures" ou un compteur atomique pour savoir quand le modèle est prêt.



### 12. Limitation du SSBO d'Instancing

*   **Fichier :** `src/bb3d/render/Renderer.cpp`

*   **Problème :** `MAX_INSTANCES` est une limite fixe. Si on a trop d'instances, les objets ne sont plus dessinés.

*   **Action :** 

    *   [ ] Gérer dynamiquement l'offset dans le SSBO ou utiliser plusieurs SSBO si nécessaire.

    *   [ ] Ajouter un warning ou une erreur explicite en cas de dépassement.



### 13. Optimisation des Transitions de Pipeline (Barrières)

*   **Fichier :** `src/bb3d/render/Renderer.cpp`

*   **Problème :** Usage de `eTopOfPipe` et `eBottomOfPipe` dans `compositeToSwapchain` et `submitAndPresent`.

*   **Action :** 

    *   [ ] Remplacer par des stages précis comme `COLOR_ATTACHMENT_OUTPUT`.
