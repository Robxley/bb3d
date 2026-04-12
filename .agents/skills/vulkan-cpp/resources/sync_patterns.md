# Aide-mémoire Synchronization & Modern Vulkan (1.4)

Synchronization 2 simplifie grandement la gestion des barrières en regroupant les masques d'accès et les étapes de pipeline dans une structure unique : `vk::DependencyInfo`.

## 1. Transition de Layout (Standard)

Remplacer les `vk::ImageMemoryBarrier` complexes par :

```cpp
vk::ImageMemoryBarrier2 barrier{};
barrier.oldLayout = vk::ImageLayout::eUndefined;
barrier.newLayout = vk::ImageLayout::eAttachmentOptimal;
barrier.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
barrier.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
barrier.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader;
barrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
barrier.image = image;
barrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

vk::DependencyInfo depInfo{};
depInfo.imageMemoryBarrierCount = 1;
depInfo.pImageMemoryBarriers = &barrier;

commandBuffer.pipelineBarrier2(depInfo);
```

## 2. Étapes de Pipeline (Common usage)

| Objectif | srcStageMask | dstStageMask |
| :--- | :--- | :--- |
| CPU -> GPU (Buffer) | `eHost` | `eTransfer` ou `eVertexAttributeInput` |
| Rendu -> Présentation | `eColorAttachmentOutput` | `eNone` (ou spécifique OS) |
| Compute -> Graphics | `eComputeShader` | `eVertexShader` |

## 3. Timeline Semaphores

Workflow pour remplacer une Fence :

```cpp
vk::SemaphoreTypeCreateInfo typeInfo{ vk::SemaphoreType::eTimeline, initialValue };
vk::SemaphoreCreateInfo createInfo{};
createInfo.pNext = &typeInfo;
auto semaphore = device.createSemaphore(createInfo);

// Signalement côté GPU
vk::TimelineSemaphoreSubmitInfo timelineInfo{};
timelineInfo.signalSemaphoreValueCount = 1;
timelineInfo.pSignalSemaphoreValues = &nextValue;

// Attente côté CPU
vk::SemaphoreWaitInfo waitInfo{};
waitInfo.semaphoreCount = 1;
waitInfo.pSemaphores = &semaphore;
waitInfo.pValues = &targetValue;
device.waitSemaphores(waitInfo, timeout);
```
