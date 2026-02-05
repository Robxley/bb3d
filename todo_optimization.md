# 🚀 Optimisations Performance - biobazard3d

Ce document liste les optimisations identifiées lors de la revue de code du moteur (Février 2026).

## 🟢 Priorité : Haute (Gain immédiat, Faible effort)

### 1. Élimination des allocations par frame (Heap Pressure)
*   **Fichier :** `src/bb3d/render/Renderer.cpp`
*   **Problème :** Les vecteurs `commands` et `instanceTransforms` dans `drawScene` sont réalloués à chaque frame.
*   **Action :** 
    *   [ ] Déplacer les vecteurs en membres privés de la classe `Renderer`.
    *   [ ] Utiliser `reserve()` au démarrage.
    *   [ ] Appeler `clear()` à chaque début de frame au lieu de détruire l'objet.
*   **Bénéfice :** Zéro allocation dynamique dans le "Hot Path" de rendu.

### 2. Staging Buffer Manager & Uploads Asynchrones
*   **Fichier :** `src/bb3d/render/Buffer.cpp`
*   **Problème :** Chaque création de Mesh/Texture fait un `waitIdle` sur le GPU pour le transfert.
*   **Action :** 
    *   [ ] Créer un `StagingManager` avec un buffer circulaire de ~64MB.
    *   [ ] Batcher les transferts de données.
    *   [ ] Utiliser des Fences pour libérer les zones du staging buffer sans bloquer le CPU.
*   **Bénéfice :** Chargement d'assets 10x plus rapide, suppression des micro-stutters lors du streaming.

## 🟡 Priorité : Moyenne (Gain CPU, Effort Modéré)

### 3. Parallélisation du Culling & Tri
*   **Fichier :** `src/bb3d/render/Renderer.cpp`
*   **Problème :** Le culling Frustum et le tri des commandes sont faits sur un seul thread.
*   **Action :** 
    *   [ ] Utiliser `JobSystem::dispatch` pour paralléliser le test d'intersection AABB/Frustum.
    *   [ ] Utiliser `std::sort` parallèle si possible ou trier par blocs.
*   **Bénéfice :** Meilleure utilisation des processeurs multi-cœurs sur les scènes chargées.

### 4. Affinage des barrières de synchronisation Vulkan
*   **Fichier :** `src/bb3d/render/Renderer.cpp` / `SwapChain.cpp`
*   **Problème :** Usage fréquent de `TOP_OF_PIPE` et `BOTTOM_OF_PIPE`.
*   **Action :** 
    *   [ ] Remplacer par des stages précis (ex: `COLOR_ATTACHMENT_OUTPUT` -> `FRAGMENT_SHADER`).
    *   [ ] Utiliser `vk::DependencyInfo` (Vulkan 1.3) pour simplifier la syntaxe.
*   **Bénéfice :** Réduction des temps d'attente "bulles" du GPU.

## 🔴 Priorité : Basse / Recherche (Expertise requise)

### 5. GPU-Driven Rendering (Indirect Draw)
*   **Problème :** Trop de draw calls côté CPU pour les scènes massives.
*   **Action :** 
    *   [ ] Implémenter un Compute Shader pour le culling (Hi-Z).
    *   [ ] Utiliser `vkCmdDrawIndexedIndirect` pour envoyer toute la scène en un seul appel.
*   **Bénéfice :** Décharge quasi totale du CPU pour la soumission des commandes de dessin.

---
*Note : Document généré par l'agent Gemini CLI.*
