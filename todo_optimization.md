# 🚀 Optimisations Performance - biobazard3d

Ce document liste les optimisations identifiées et suivies pour le moteur.

## 🟢 Priorité : Haute (Gain immédiat, Faible effort)

### 1. Élimination des allocations par frame (Heap Pressure)
*   **Action :** Déplacer les vecteurs de commandes et de transforms en membres de classe, utiliser `reserve()` et `clear()`.
*   **Statut :** ✅ Terminé.

### 2. Gestion Dynamique des Descripteurs
*   **Problème :** Le `DescriptorPool` global a une taille fixe.
*   **Action :** Créer un `DescriptorAllocator` gérant plusieurs pools dynamiquement.
*   **Statut :** ⏳ À faire.

### 3. Réduction de l'empreinte RAM des Meshs
*   **Action :** Appeler automatiquement `releaseCPUData()` après l'upload GPU pour les meshes statiques.
*   **Statut :** ⏳ À faire (Fonctionnalité existante, intégration auto requise).

### 4. Triple Buffering des Matériaux
*   **Problème :** Freezes Vulkan dus à la mise à jour de descripteurs en cours d'utilisation par le GPU.
*   **Action :** Implémenter un tableau de Descriptor Sets par matériau (un par frame).
*   **Statut :** ✅ Terminé.

## 🟡 Priorité : Moyenne (Gain CPU, Effort Modéré)

### 5. Async Texture Upload (Streaming)
*   **Problème :** `waitIdle` bloque le CPU lors de la création de textures.
*   **Action :** Remplacer `waitIdle` par des Fences et utiliser une `TransferQueue` dédiée.
*   **Statut :** 🚀 En cours (Prochaine étape).

### 6. Parallélisation du Culling & Tri
*   **Action :** Utiliser `JobSystem::dispatch` pour le test d'intersection AABB/Frustum.
*   **Statut :** ✅ Terminé.

### 7. Optimisation Chargement Modèles
*   **Action :** Charger les textures en parallèle via `JobSystem` lors du chargement du modèle.
*   **Statut :** ⏳ À faire.

## 🔴 Priorité : Basse / Recherche (Expertise requise)

### 8. GPU-Driven Rendering
*   **Action :** Implémenter `vkCmdDrawIndexedIndirect` et Culling GPU (Compute Shader).
*   **Statut :** 🔍 Recherche.

### 9. Initialisation Réactive de la Physique
*   **Action :** Utiliser les observers EnTT pour créer les corps Jolt au moment de l'ajout du composant.
*   **Statut :** ⏳ À faire.

---
*Dernière mise à jour : 11 Février 2026*
