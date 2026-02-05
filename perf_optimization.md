# Performance Optimization Strategy - biobazard3d

**Version :** 1.0 (Phase d'Analyse Post-Stabilisation)  
**Objectif :** Atteindre des performances de classe production en minimisant l'overhead CPU et en maximisant l'utilisation du GPU.

---

## 1. Réduction de l'Overhead CPU (Hot Path)

### ⚠️ Goulot : Allocation de la Draw List
Actuellement, `Renderer::render()` alloue un `std::vector<RenderCommand>` et le trie à chaque frame.
- **Optimisation :** Utiliser un **Linear Allocator (Arena)** persistant. On alloue une large zone de mémoire au démarrage, et on "reset" simplement un pointeur à chaque frame.
- **Gain :** Zéro allocation/désallocation dynamique dans la boucle de rendu.

### ⚠️ Goulot : Tri Systématique
Le tri par `MaterialType` puis `Material*` est effectué sur toute la scène.
- **Optimisation :** Implémenter une **Draw List Cache**. Si la scène n'a pas changé (pas d'entité ajoutée/supprimée), on réutilise l'ordre de la frame précédente. Seules les matrices de transformation sont mises à jour.

### ⚠️ Goulot : Appels de Scripts
L'utilisation de `std::function` dans `NativeScriptComponent` ajoute un overhead d'indirection.
- **Optimisation :** Pour les systèmes critiques, préférer des **Systems EnTT** dédiés qui itèrent directement sur les composants sans passer par des lambdas opaques.

---

## 2. Data-Oriented Design (DOD) & Cache

### 🚀 Vectorisation des Transformations
Actuellement, chaque entité calcule sa `mat4` via `getTransform()`.
- **Optimisation :** Maintenir un tableau contigu de matrices pour toutes les entités actives. Utiliser le multithreading (`JobSystem`) pour calculer toutes les matrices d'un coup (SIMD-friendly).
- **GPU Sync :** Envoyer ce tableau en une seule fois via un **SSBO (Shader Storage Buffer Object)** au lieu de multiples push constants.

### 🚀 Hachage des Ressources
Le cache de matériaux utilise des `std::string` comme clés.
- **Optimisation :** Utiliser des `uint64_t` (hachage du chemin) ou `std::string_view` pour éviter les copies et allocations lors des recherches.

---

## 3. Optimisations Vulkan & GPU-Driven

### 🚀 Instancing Automatique
Réduire les Draw Calls pour les objets identiques.
- **Implémentation :** Regrouper les `RenderCommand` ayant le même `Mesh`. Envoyer un tableau de matrices au shader et utiliser `vkCmdDrawIndexedIndirect` ou `vkCmdDrawIndexed` avec `instanceCount`.

### 🚀 GPU-Driven Culling (Compute Shaders)
Actuellement, le GPU reçoit tous les objets, même ceux hors-champ.
- **Optimisation :** Utiliser un Compute Shader pour effectuer le **Frustum Culling** et l'**Occlusion Culling** (via Hi-Z buffer).
- **Workflow :** Le Compute Shader génère la liste des commandes de dessin directement dans un buffer (`Indirect Buffer`) consommé par `vkCmdDrawIndexedIndirect`. Le CPU ne gère plus la visibilité.

---

## 4. Instrumentation Tracy Avancée

Pour mesurer ces gains, l'instrumentation doit monter en gamme :
- **Zones GPU :** Intégrer `TracyVulkan.hpp` pour corréler les temps CPU avec l'exécution réelle sur le GPU.
- **Tracking Mémoire :** Utiliser les macros `TracyAlloc` et `TracyFree` dans le `VulkanContext` (VMA) pour visualiser l'empreinte mémoire en temps réel.
- **Plots Personnalisés :** Tracer le nombre de Draw Calls et le nombre de triangles par frame.

---

## 5. C++20 & Standard Library

- **std::ranges :** Remplacer les boucles manuelles par des vues (`std::views::filter`, `std::views::transform`) pour la collecte des commandes de rendu, permettant potentiellement une meilleure optimisation par le compilateur.
- **Coroutines :** Idéal pour le chargement d'assets asynchrone sans bloquer le thread principal (`co_await assets.load(...)`).

---

## Conclusion & Priorités
1. **Priorité 1 :** Remplacement du `std::vector` de commandes par une Arena (Gain CPU immédiat).
2. **Priorité 2 :** Implémentation du SSBO pour les transformations et Instancing basique.
3. **Priorité 3 :** GPU-Driven Culling pour les scènes à grande échelle.
