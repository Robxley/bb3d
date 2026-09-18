# Document de Conception : Migration Complète Synchronization2 (pipelineBarrier2 & vk::DependencyInfo)

- **Auteur :** @Antigravity
- **Date :** 2026-09-18
- **Statut :** PROPOSED (Ready for Architecture Review)
- **Jalon :** Jalon 2 (Socle Vulkan 1.3 / 1.4 Moderne & Synchronisation) - Chantier 2

---

## 1. Contexte & Problématique

Dans le cadre du **Jalon 2, Chantier 1**, nous avons modernisé le backend Vulkan (`VulkanContext`) avec `vk::StructureChain` et activé officiellement la fonctionnalité core `synchronization2` (Vulkan 1.3/1.4).

Cependant, le code de rendu existant utilise encore l'API legacy Vulkan 1.0 (`vk::CommandBuffer::pipelineBarrier`) à **18 endroits distincts** :
- **14 occurrences dans `Renderer.cpp`** : barrières de layout pour la swapchain, le render target offscreen, les cascades d'ombres (CSM), la passe de composition post-process et les copies de capture d'écran.
- **4 occurrences dans `Texture.cpp`** : transitions de chargement d'image et transitions mip-level par mip-level lors de `generateMipmaps`.

### Pourquoi migrer vers Synchronization2 (`pipelineBarrier2`) ?
1. **Masques d'étapes et d'accès 64-bit (`vk::PipelineStageFlagBits2`, `vk::AccessFlagBits2`)** :
   L'ancienne API 32-bit souffre d'un manque d'expressivité et oblige à recourir à des pseudo-stages vagues comme `eTopOfPipe` ou `eBottomOfPipe`, provoquant des bulles de pipeline ou des attentes prématurées.
2. **Unification et Batching via `vk::DependencyInfo`** :
   Plusieurs barrières consécutives (ex : transition simultanée de l'attachement couleur et de l'attachement profondeur) sont aujourd'hui émises sous forme de multiples appels `cb.pipelineBarrier()`. `pipelineBarrier2` permet de regrouper ces barrières dans un tableau au sein d'une seule structure `vk::DependencyInfo`, réduisant la charge CPU du pilote et optimisant l'exécution GPU.
3. **Respect strict des standards du projet (`AGENTS.md`)** :
   La directive stipule : *"Bannir pipelineBarrier (Vulkan 1.0). Utiliser exclusivement pipelineBarrier2 avec vk::DependencyInfo et vk::ImageMemoryBarrier2."*

---

## 2. Inventaire Détaillé des 18 Barrières & Stratégie de Migration

### 2.1. `Renderer.cpp` : Passe Principale & Attachements (`Renderer::render()`)

#### A. Rendu Offscreen (RenderTarget) : Initialisation Couleur + Profondeur
- **Code Actuel (L491-496) :** 2 appels `cb.pipelineBarrier` distincts.
  - `rtBarrier` : `Undefined` -> `ColorAttachmentOptimal`
  - `dBarrier` : `Undefined` -> `DepthStencilAttachmentOptimal`
- **Migration Sync2 (Batchée) :**
  - `rtBarrier` : `srcStageMask = ColorAttachmentOutput`, `dstStageMask = ColorAttachmentOutput`, `dstAccessMask = ColorAttachmentWrite`
  - `dBarrier` : `srcStageMask = EarlyFragmentTests`, `dstStageMask = EarlyFragmentTests`, `dstAccessMask = DepthStencilAttachmentWrite`
  - **Optimisation :** Regroupées dans un `std::array<vk::ImageMemoryBarrier2, 2>` transmis à un unique `cb.pipelineBarrier2(depInfo)`.

#### B. Swapchain Directe (Non-Offscreen) : Initialisation Couleur + Profondeur
- **Code Actuel (L520-525) :** 2 appels `cb.pipelineBarrier` distincts (`swBarrier` + `dBarrier`).
- **Migration Sync2 (Batchée) :** Regroupement dans un unique appel `cb.pipelineBarrier2(depInfo)` avec `std::array<vk::ImageMemoryBarrier2, 2>`.

#### C. Transition Swapchain Pré-Éditeur (L507-510)
- **Code Actuel :** `eTopOfPipe` -> `eColorAttachmentOutput`
- **Migration Sync2 :** Remplacement de `eTopOfPipe` par `vk::PipelineStageFlagBits2::eColorAttachmentOutput`.

#### D. RenderTarget Post-Rendu vers Shader Read (L512-513)
- **Code Actuel :** `ColorAttachmentOptimal` -> `ShaderReadOnlyOptimal`
- **Migration Sync2 :**
  - `srcStageMask = ColorAttachmentOutput`, `srcAccessMask = ColorAttachmentWrite`
  - `dstStageMask = FragmentShader`, `dstAccessMask = ShaderRead`

---

### 2.2. `Renderer.cpp : submitAndPresent()` (Transition de Présentation)

- **Code Actuel (L540-541) :**
  `ColorAttachmentOptimal` -> `PresentSrcKHR`, avec `eColorAttachmentOutput` -> `eBottomOfPipe`.
- **Migration Sync2 :**
  - Dans Synchronization2, la transition vers `PresentSrcKHR` ne doit plus cibler le pseudo-stage `eBottomOfPipe` :
    - `srcStageMask = ColorAttachmentOutput`, `srcAccessMask = ColorAttachmentWrite`
    - `dstStageMask = ColorAttachmentOutput`, `dstAccessMask = {}`
    - `newLayout = vk::ImageLayout::ePresentSrcKHR`

---

### 2.3. `Renderer.cpp : compositeToSwapchain()`

- **Code Actuel (L668-673) :** 2 appels `cb.pipelineBarrier` distincts :
  1. `swapImage` : `Undefined` -> `ColorAttachmentOptimal`
  2. `rtImage` : `ColorAttachmentOptimal` -> `ShaderReadOnlyOptimal`
- **Migration Sync2 (Batchée) :**
  - Regroupement des deux barrières dans un unique `vk::DependencyInfo` contenant `std::array<vk::ImageMemoryBarrier2, 2>`.
- **Code Actuel (L690-691) :** `resetBarrier` de `rtImage` (`ShaderReadOnlyOptimal` -> `ColorAttachmentOptimal`).
  - `srcStageMask = FragmentShader`, `srcAccessMask = ShaderRead`
  - `dstStageMask = ColorAttachmentOutput`, `dstAccessMask = ColorAttachmentWrite`

---

### 2.4. `Renderer.cpp : renderShadows()` (Cascaded Shadow Maps)

- **Pré-Passe d'Ombres (L910-918) :**
  - `m_shadowDepthImage` : `Undefined` -> `DepthStencilAttachmentOptimal` sur toutes les cascades.
  - `srcStageMask = EarlyFragmentTests | LateFragmentTests`
  - `dstStageMask = EarlyFragmentTests`, `dstAccessMask = DepthStencilAttachmentWrite`
- **Post-Passe d'Ombres (L1000-1006) :**
  - `m_shadowDepthImage` : `DepthStencilAttachmentOptimal` -> `DepthStencilReadOnlyOptimal`.
  - `srcStageMask = LateFragmentTests`, `srcAccessMask = DepthStencilAttachmentWrite`
  - `dstStageMask = FragmentShader`, `dstAccessMask = ShaderRead`

---

### 2.5. `Renderer.cpp : saveScreenshot()` / `copyImageToBuffer()`

- **Transition vers TransferSrc (L1411-1412) :**
  - `PresentSrcKHR` -> `TransferSrcOptimal`
  - `srcStageMask = Transfer`, `dstStageMask = Transfer`, `dstAccessMask = TransferRead`
- **Restauration vers PresentSrc (L1418-1422) :**
  - `TransferSrcOptimal` -> `PresentSrcKHR`
  - `srcStageMask = Transfer`, `srcAccessMask = TransferRead`, `dstStageMask = Transfer`, `dstAccessMask = MemoryRead`

---

### 2.6. `Texture.cpp : transitionLayout()` & `generateMipmaps()`

- **`Texture::transitionLayout()` (L347-368) :**
  - `Undefined` -> `TransferDstOptimal` :
    - `srcStageMask = AllCommands`, `dstStageMask = Transfer`, `dstAccessMask = TransferWrite`
  - `TransferDstOptimal` -> `ShaderReadOnlyOptimal` :
    - `srcStageMask = Transfer`, `srcAccessMask = TransferWrite`, `dstStageMask = FragmentShader`, `dstAccessMask = ShaderRead`
- **`Texture::generateMipmaps()` (L306-345) :**
  - Blit Src transition : `TransferDstOptimal` -> `TransferSrcOptimal` (`TransferWrite` -> `TransferRead`).
  - Blit Dst transition : `TransferSrcOptimal` -> `ShaderReadOnlyOptimal` (`TransferRead` -> `ShaderRead`).
  - Dernier niveau de mip : `TransferDstOptimal` -> `ShaderReadOnlyOptimal` (`TransferWrite` -> `ShaderRead`).

---

## 3. Stratégie de Test TDD & Validation

1. **Test Dédié TDD (`tests/unit_test_32_synchronization2.cpp`) :**
   - Écriture préalable d'un test automatisé non-interactif validant les scénarios de synchronisation clés :
     - Soumission d'une commande `pipelineBarrier2` avec batching d'images multiples.
     - Transition d'image avec vérification de non-régression via les Validation Layers Vulkan.
     - Chargement complet d'une texture avec génération de mipmaps via `Texture` moderne.
2. **Exécution CTest :**
   - Validation que tous les tests de rendu (`unit_test_02_vulkan_init`, `unit_test_03_swapchain`, `unit_test_24_auto_offscreen`, `unit_test_shadows`, `unit_test_26_picking`) s'exécutent avec 0 erreur de synchronisation.
3. **Zéro Warning Compilateur & Zero Validation Layer Error.**

---

## 4. Plan de Revue Conjointe (Mistral Vibe)

Avant l'implémentation, ce document sera soumis pour avis consultatif à l'agent Vibe CLI (`bb3d-reviewer`) en mode lecture seule (`max_turns=40`, modèle `glm-5.2`), afin de confronter les masques de stages et s'assurer de l'absence de deadlocks GPU.
