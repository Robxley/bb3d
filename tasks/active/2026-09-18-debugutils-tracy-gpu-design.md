# Design Doc : Instrumentation DebugUtils & Profiling Tracy GPU

- **Date :** 2026-09-18
- **Auteur :** Antigravity (@dev)
- **Statut :** PROPOSÉ (En cours de validation Brainstorming)
- **Jalon :** Jalon 2 - Socle Vulkan 1.3/1.4 Moderne (Chantier 4)

---

## 1. Contexte & Problématique

Actuellement dans `bb3d` :
1. **DebugUtils :** L'extension `VK_EXT_debug_utils` n'est activée que lorsque `enableValidationLayers == true` (`VulkanContext.cpp:58`). Aucun balisage de command buffer (`vkCmdBeginDebugUtilsLabelEXT` / `vkCmdEndDebugUtilsLabelEXT`) ni nommage explicite des ressources GPU (`vkSetDebugUtilsObjectNameEXT`) n'est présent dans le moteur. Dans RenderDoc ou Nsight, toutes les passes apparaissent comme une longue suite de draw calls anonymes, et les erreurs de validation layer ne citent que des handles numériques hexadécimaux bruts.
2. **Profiling Tracy :** Tracy CPU est supporté via `BB_PROFILE_SCOPE` et `BB_PROFILE_FRAME` dans `Core.hpp`, mais **aucun profiling GPU** n'est branché. Les goulets d'étranglement GPU (coût de la passe d'ombres cascadaitée, coût du shading PBR, coût du composite offscreen, etc.) ne peuvent pas être mesurés avec précision sur la timeline GPU Tracy.

---

## 2. Objectifs & Critères de Réussite

1. **Activation conditionnelle et robuste de `VK_EXT_debug_utils` :**
   - Détection dynamique via `enumerateInstanceExtensionProperties`.
   - Activation si demandée par la configuration, si les validation layers sont actives, ou si un outil de diagnostic (RenderDoc/Nsight) est détecté.
   - Initialisation des pointeurs de fonctions de dispatch dans `VulkanContext`.
2. **RAII Scoped Debug Labels :**
   - Classe C++20 `ScopedDebugLabel` garantissant la parité stricte `begin` / `end` sur les command buffers même en cas de sortie anticipée (`return`) ou d'exception.
   - Méthode `setDebugObjectName` type-safe pour nommer buffers, images, pipelines, render targets.
3. **Profiling Tracy GPU (`TracyVulkan.hpp`) :**
   - Initialisation d'un `TracyVkCtx` lié à la file graphique (`m_graphicsQueue`) et au `m_device`.
   - Collecte par frame via `TracyVkCollect(m_tracyVkCtx, cb)`.
   - Destruction propre de `TracyVkCtx` dans `cleanup()`.
   - Macro/Wrapper `BB_GPU_ZONE` ou classe unifiée `ScopedGpuMarker` combinant le label DebugUtils et la zone Tracy GPU.
4. **Balisage des passes de rendu majeures dans `Renderer.cpp` :**
   - `Frame Render`
   - `Shadow Pass` (avec sous-zones par cascade)
   - `Skybox Pass`
   - `Scene PBR Pass`
   - `Composite Pass`
   - `Picking Pass`
5. **Opacité de l'API Publique :**
   - Aucune fuite de types Vulkan (`vk::*`) ou Tracy (`tracy::*`) dans les headers publics client (`Engine`, `Scene`, `Component`).
6. **Tests & Validation :**
   - Test unitaire TDD `tests/unit_test_34_debug_utils_profiling.cpp`.
   - Suite CTest 100% PASS (16/16), 0 warning MSVC `/W4`.
   - Revue de code indépendante via Mistral Vibe CLI (`glm-5.2`).

---

## 3. Options d'Architecture Explorées

### Option A : Découplage Modulaire (ScopedDebugLabel + ScopedGpuZone séparés)
- `ScopedDebugLabel` gère uniquement `VK_EXT_debug_utils`.
- `ScopedGpuZone` gère `TracyVkZone`.
- Possibilité d'appeler l'un sans l'autre.
- *Avantages :* Très modulaire, Tracy peut être désactivé sans impacter RenderDoc.
- *Inconvénients :* Nécessite deux lignes de code par passe si on veut les deux.

### Option B : Macro Pure (`BB_DEBUG_LABEL` & `BB_GPU_ZONE`)
- Macros inspirées de `ZoneScopedN` / `BB_PROFILE_SCOPE`.
- *Avantages :* 1 seule ligne par passe.
- *Inconvénients :* Moins idiomatique C++20 RAII, macros sensibles au scoping et conflits de nom.

### Option C (Recommandée) : Hybride C++20 RAII avec Helper Unifié
- Classe fondamentale `ScopedDebugLabel` dans `bb3d` pour RenderDoc/Nsight (toujours disponible si extension active, 0 coût sinon).
- Classe/Macro `ScopedGpuMarker` / `BB_GPU_ZONE(renderer, cb, name, color)` qui déclenche à la fois `ScopedDebugLabel` ET `TracyVkZone` sur le bloc de code.
- Nommage d'objets `VulkanContext::setObjectName(uint64_t handle, vk::ObjectType type, std::string_view name)`.
- *Avantages :* Meilleure ergonomie, robustesse RAII totale, instrumentation 1 ligne pour les deux systèmes simultanément.

---

## 4. Architecture Détaillée

### 4.1. VulkanContext : Détection et Fonctions DebugUtils
```cpp
// Dans VulkanContext.hpp
bool m_debugUtilsSupported = false;
void setDebugObjectName(uint64_t objectHandle, vk::ObjectType objectType, std::string_view name);

template <typename T>
void setObjectName(T handle, std::string_view name) {
    setDebugObjectName(reinterpret_cast<uint64_t>(static_cast<typename T::NativeType>(handle)), T::objectType, name);
}

void cmdBeginDebugLabel(vk::CommandBuffer cb, std::string_view name, std::array<float, 4> color = {0.2f, 0.6f, 1.0f, 1.0f});
void cmdEndDebugLabel(vk::CommandBuffer cb);
```

### 4.2. RAII ScopedDebugLabel
```cpp
class ScopedDebugLabel {
public:
    ScopedDebugLabel(VulkanContext& ctx, vk::CommandBuffer cb, std::string_view name, std::array<float, 4> color = {0.2f, 0.6f, 1.0f, 1.0f})
        : m_ctx(ctx), m_cb(cb), m_active(ctx.isDebugUtilsSupported()) {
        if (m_active) {
            m_ctx.cmdBeginDebugLabel(m_cb, name, color);
        }
    }
    ~ScopedDebugLabel() {
        if (m_active) {
            m_ctx.cmdEndDebugLabel(m_cb);
        }
    }
    // Delete copy, allow move
};
```

### 4.3. Intégration Tracy GPU dans Renderer
```cpp
// Dans Renderer.hpp
#if defined(BB_PROFILE)
    tracy::VkCtx* m_tracyGpuContext = nullptr;
#endif

// Dans Renderer.cpp
// À l'init :
#if defined(BB_PROFILE)
    // Allocation d'un command buffer one-time pour le calibrage/init Tracy
    vk::CommandBuffer initCb = m_context.beginSingleTimeCommands();
    m_tracyGpuContext = TracyVkContext(
        static_cast<VkPhysicalDevice>(m_context.getPhysicalDevice()),
        static_cast<VkDevice>(m_context.getDevice()),
        static_cast<VkQueue>(m_context.getGraphicsQueue()),
        static_cast<VkCommandBuffer>(initCb)
    );
    m_context.endSingleTimeCommands(initCb);
#endif

// À chaque frame dans submitAndPresent() :
#if defined(BB_PROFILE)
    if (m_tracyGpuContext) {
        TracyVkCollect(m_tracyGpuContext, cb);
    }
#endif

// Au cleanup :
#if defined(BB_PROFILE)
    if (m_tracyGpuContext) {
        TracyVkDestroy(m_tracyGpuContext);
        m_tracyGpuContext = nullptr;
    }
#endif
```

### 4.4. Macros d'instrumentation dans Core.hpp / Renderer.hpp
```cpp
#if defined(BB_PROFILE)
    #define BB_GPU_ZONE(tracyCtx, cb, name) \
        ScopedDebugLabel tracyConcat(__debug_label_, __LINE__)(m_context, cb, name); \
        TracyVkZone(tracyCtx, static_cast<VkCommandBuffer>(cb), name)
#else
    #define BB_GPU_ZONE(tracyCtx, cb, name) \
        ScopedDebugLabel tracyConcat(__debug_label_, __LINE__)(m_context, cb, name)
#endif
```

---

## 5. Plan de Test & Vérification

- `tests/unit_test_34_debug_utils_profiling.cpp` :
  1. Test 1 : Vérifier la détection et initialisation de DebugUtils (`m_context.isDebugUtilsSupported()`).
  2. Test 2 : Vérifier le nommage d'objets (`setObjectName`) sans crash ni violation de validation layer.
  3. Test 3 : Vérifier `ScopedDebugLabel` (imbrication de labels, parité begin/end, zéro crash).
  4. Test 4 : Vérifier l'initialisation et la collecte Tracy GPU si `BB_PROFILE` actif.
- CTest complet 16/16 PASS.
- Validation Layers Khronos : 0 warning, 0 error.
