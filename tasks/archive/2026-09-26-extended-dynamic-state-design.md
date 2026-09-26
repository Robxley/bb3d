# Design Doc — Chantier 6 : Extended Dynamic State

- **Date :** 2026-09-26
- **Auteur :** Agent Implémenteur
- **Branche :** `feat/extended-dynamic-state`

---

## 1. Contexte & Objectif

### Problème actuel

Dans `GraphicsPipeline.cpp`, seuls 3 états sont dynamiques : `eViewport`, `eScissor`, `eDepthBias`.
Les états `CullMode`, `FrontFace`, `DepthTest`, `DepthWrite`, `DepthCompareOp` et `PrimitiveTopology` sont baked dans chaque pipeline au moment de la compilation.

**Conséquence :** Pour un matériau avec `cullMode = None` (skybox, shadow) ou `depthWrite = false` (transparent), il faut compiler un pipeline entier distinct. Actuellement le renderer compte **8+ pipelines** pour des variantes triviales.

### Objectif

Activer `VK_EXT_extended_dynamic_state` (promu en Vulkan 1.3 Core) pour rendre dynamiques :
- `eCullMode`
- `eFrontFace`
- `eDepthTestEnable`
- `eDepthWriteEnable`
- `eDepthCompareOp`
- `ePrimitiveTopology`

Et binder ces états dans les command buffers via `cb.setCullMode()`, `cb.setFrontFace()`, etc.

---

## 2. Analyse Technique

### Feature Flag

`extendedDynamicState` n'est **pas** un champ de `VkPhysicalDeviceVulkan13Features`.
Il existe dans `VkPhysicalDeviceExtendedDynamicStateFeaturesEXT` (EXT) — mais toutes les commandes correspondantes (`vkCmdSetCullMode`, `vkCmdSetDepthTestEnable`, etc.) sont **Vulkan 1.3 Core** (promues sans extension).

**Stratégie retenue :** Activer la feature via un chaînage `VkPhysicalDeviceExtendedDynamicStateFeaturesEXT` dans la `StructureChain` de création du device (Vulkan 1.3 et 1.4).

### Approche : Feature optionnelle + fallback gracieux

Si le device ne supporte pas `extendedDynamicState`, le renderer repasse sur les pipelines baked existants (mode dégradé sans crash). La feature est cochée dans `EnabledFeatures`.

---

## 3. Fichiers Modifiés

| Fichier | Modification |
|---|---|
| `include/bb3d/render/VulkanContext.hpp` | Ajouter `extendedDynamicState` à `EnabledFeatures` |
| `src/bb3d/render/VulkanContext.cpp` | Chaîner `VkPhysicalDeviceExtendedDynamicStateFeaturesEXT` dans Vulkan 1.3 et 1.4 |
| `src/bb3d/render/GraphicsPipeline.cpp` | Ajouter 5+ états dynamiques, supprimer les états statiques correspondants |
| `src/bb3d/render/Renderer.cpp` | Binder les états dynamiques dans `drawScene()`, `renderShadows()`, `renderSkybox()` |
| `tests/unit_test_37_extended_dynamic_state.cpp` | Test TDD de validation |

---

## 4. Design de l'API d'activation

```cpp
// VulkanContext.hpp — EnabledFeatures
struct EnabledFeatures {
    // ... existing fields
    bool extendedDynamicState = false; // VK_EXT_extended_dynamic_state / Vulkan 1.3 Core commands
};
```

```cpp
// VulkanContext.cpp — initLogicalDevice (Vulkan 1.3 + 1.4)
// Dans les deux branches (isVulkan14 true/false), ajouter dans la queryChain :
vk::StructureChain<
    vk::PhysicalDeviceFeatures2,
    vk::PhysicalDeviceVulkan11Features,
    vk::PhysicalDeviceVulkan12Features,
    vk::PhysicalDeviceVulkan13Features,
    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT  // <-- NOUVEAU
> queryChain;

// Si supportée : activer dans createChain
auto& extDyn = createChain.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
extDyn.extendedDynamicState = VK_TRUE;
m_enabledFeatures.extendedDynamicState = true;
```

---

## 5. Design de GraphicsPipeline

```cpp
// Nouveaux dynamic states (Vulkan 1.3 Core si extendedDynamicState supporté)
static constexpr std::array<vk::DynamicState, 9> kDynamicStatesExtended = {
    vk::DynamicState::eViewport,
    vk::DynamicState::eScissor,
    vk::DynamicState::eDepthBias,
    vk::DynamicState::eCullMode,
    vk::DynamicState::eFrontFace,
    vk::DynamicState::eDepthTestEnable,
    vk::DynamicState::eDepthWriteEnable,
    vk::DynamicState::eDepthCompareOp,
    vk::DynamicState::ePrimitiveTopology,
};
```

Quand `extendedDynamicState` est actif :
- `CullMode`, `FrontFace` → state ignoré dans `VkPipelineRasterizationStateCreateInfo` (overridé runtime)
- `depthTestEnable`, `depthWriteEnable`, `depthCompareOp` → ignorés dans `VkPipelineDepthStencilStateCreateInfo` (overridé runtime)

---

## 6. Design du Renderer (Command Buffer Binding)

Avant de binder un pipeline, émettre les états dynamiques. Chaque MaterialType porte implicitement ses états via `RenderCommand`. Le renderer stocke les états courants et ne les ré-émet que si changés.

```cpp
// Dans drawScene() — à chaque changement de pipeline :
if (m_context.getEnabledFeatures().extendedDynamicState) {
    cb.setCullMode(cmd.type == MaterialType::Skybox ? vk::CullModeFlagBits::eNone : vk::CullModeFlagBits::eBack);
    cb.setFrontFace(vk::FrontFace::eCounterClockwise);
    cb.setDepthTestEnable(cmd.type != MaterialType::Skybox);
    cb.setDepthWriteEnable(cmd.material && cmd.material->getBlendMode() == BlendMode::Opaque);
    cb.setDepthCompareOp(cmd.type == MaterialType::Skybox ? vk::CompareOp::eAlways : vk::CompareOp::eLess);
    cb.setPrimitiveTopology(cmd.type == MaterialType::Highlight ? vk::PrimitiveTopology::eLineList : vk::PrimitiveTopology::eTriangleList);
}
```

---

## 7. Critères d'Acceptation TDD

Le test `unit_test_37` doit vérifier :
1. `VulkanContext` expose `getEnabledFeatures().extendedDynamicState` cohérent avec le support hardware.
2. `GraphicsPipeline` construit sans erreur avec les états dynamiques étendus.
3. Les commandes `cb.setCullMode`, `cb.setDepthTestEnable` etc. sont appelées sans erreur dans un command buffer valide (via `Engine::Create` + frame fictive).
