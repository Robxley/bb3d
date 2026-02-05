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

### 2. Staging Buffer Manager & Uploads Asynchrones
*   **Fichier :** `src/bb3d/render/Buffer.cpp`
*   **Problème :** Chaque création de Mesh/Texture fait un `waitIdle` sur le GPU pour le transfert.
*   **Action :** 
    *   [x] Créer un `StagingManager` avec un buffer circulaire de ~64MB.
    *   [x] Batcher les transferts de données.
    *   [x] Utiliser des Fences pour libérer les zones du staging buffer sans bloquer le CPU.
*   **Bénéfice :** Chargement d'assets 10x plus rapide, suppression des micro-stutters lors du streaming. (Terminé : Février 2026)

## 🟡 Priorité : Moyenne (Gain CPU, Effort Modéré)

### 3. Parallélisation du Culling & Tri
*   **Fichier :** `src/bb3d/render/Renderer.cpp`
*   **Problème :** Le culling Frustum et le tri des commandes sont faits sur un seul thread.
*   **Action :** 
    *   [ ] Utiliser `JobSystem::dispatch` pour paralléliser le test d'intersection AABB/Frustum.
    *   [ ] Utiliser `std::sort` parallèle si possible ou trier par blocs.
*   **Bénéfice :** Meilleure utilisation des processeurs multi-cœurs sur les scènes chargées. (En cours)

### 4. Affinage des barrières de synchronisation Vulkan
*   **Fichier :** `src/bb3d/render/Renderer.cpp` / `SwapChain.cpp`
*   **Problème :** Usage fréquent de `TOP_OF_PIPE` et `BOTTOM_OF_PIPE`.
*   **Action :** 
    *   [ ] Remplacer par des stages précis (ex: `COLOR_ATTACHMENT_OUTPUT` -> `FRAGMENT_SHADER`).
    *   [ ] Utiliser `vk::DependencyInfo` (Vulkan 1.3) pour simplifier la syntaxe.
*   **Bénéfice :** Réduction des temps d'attente "bulles" du GPU. (À faire)

## 🔴 Priorité : Basse / Recherche (Expertise requise)

### 5. GPU-Driven Rendering (Indirect Draw & GPU Culling)
*   **Problème :** Trop de draw calls côté CPU et culling CPU limitant pour les scènes massives.
*   **Action :** 
    *   [ ] Implémenter un SSBO pour les matrices d'objets.
    *   [ ] Utiliser `vkCmdDrawIndexedIndirect` pour envoyer les commandes de dessin groupées.
    *   [ ] Implémenter un Compute Shader pour le culling Frustum.
*   **Bénéfice :** Décharge totale du CPU pour la visibilité et la soumission des commandes de dessin. (À faire)

---
*Note : Document mis à jour par l'agent Gemini CLI.*