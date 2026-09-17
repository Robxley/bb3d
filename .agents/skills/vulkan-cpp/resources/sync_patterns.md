# Aide-mémoire Synchronization2 & Modern Vulkan (1.3 / 1.4)

Synchronization 2 (`VK_KHR_synchronization2` ou Vulkan 1.3 Core) résout les limitations de l'ancienne API de synchronisation en étendant les masques d'étapes et d'accès en 64 bits et en unifiant les soumissions via `vk::DependencyInfo`.

---

## 1. Transition de Layout d'Image (`vk::ImageMemoryBarrier2`)

Pour changer le layout d'une image (ex: Undefined -> Color Attachment Optimal) :

```cpp
vk::ImageMemoryBarrier2 barrier{};
barrier.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
barrier.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
barrier.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader;
barrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
barrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
barrier.image = image;
barrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

vk::DependencyInfo depInfo{};
depInfo.imageMemoryBarrierCount = 1;
depInfo.pImageMemoryBarriers = &barrier;

commandBuffer.pipelineBarrier2(depInfo);
```

---

## 2. Passe d'Ombres (Depth Write -> Fragment Shader Read)

Transition indispensable entre la passe de rendu de shadow maps et la passe principale PBR :

```cpp
vk::ImageMemoryBarrier2 depthBarrier{};
depthBarrier.srcStageMask = vk::PipelineStageFlagBits2::eLateFragmentTests;
depthBarrier.srcAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
depthBarrier.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader;
depthBarrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
depthBarrier.oldLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
depthBarrier.newLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
depthBarrier.image = shadowDepthImage;
depthBarrier.subresourceRange = { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, cascadeCount };

vk::DependencyInfo depInfo{};
depInfo.imageMemoryBarrierCount = 1;
depInfo.pImageMemoryBarriers = &depthBarrier;

commandBuffer.pipelineBarrier2(depInfo);
```

---

## 3. Transfert CPU/Staging -> GPU Buffer (`vk::BufferMemoryBarrier2`)

Après une commande de copie `copyBuffer` depuis un staging buffer vers un vertex ou index buffer :

```cpp
vk::BufferMemoryBarrier2 bufferBarrier{};
bufferBarrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
bufferBarrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
bufferBarrier.dstStageMask = vk::PipelineStageFlagBits2::eVertexAttributeInput;
bufferBarrier.dstAccessMask = vk::AccessFlagBits2::eVertexAttributeRead;
bufferBarrier.buffer = vertexBuffer;
bufferBarrier.offset = 0;
bufferBarrier.size = VK_WHOLE_SIZE;

vk::DependencyInfo depInfo{};
depInfo.bufferMemoryBarrierCount = 1;
depInfo.pBufferMemoryBarriers = &bufferBarrier;

commandBuffer.pipelineBarrier2(depInfo);
```

---

## 4. Timeline Semaphores (Synchronisation Asynchrone)

Remplace les sémaphores binaires et les clôtures (fences) multiples par un compteur 64 bits monotone.

### Création du Timeline Semaphore :
```cpp
vk::SemaphoreTypeCreateInfo typeInfo{ vk::SemaphoreType::eTimeline, 0 /* valeur initiale */ };
vk::SemaphoreCreateInfo createInfo{};
createInfo.pNext = &typeInfo;
auto timelineSemaphore = device.createSemaphore(createInfo);
```

### Signalement côté GPU (Queue Submit) :
```cpp
uint64_t signalValue = currentFrameCount;

vk::TimelineSemaphoreSubmitInfo timelineInfo{};
timelineInfo.signalSemaphoreValueCount = 1;
timelineInfo.pSignalSemaphoreValues = &signalValue;

vk::SubmitInfo submitInfo{};
submitInfo.pNext = &timelineInfo;
submitInfo.commandBufferCount = 1;
submitInfo.pCommandBuffers = &commandBuffer;

queue.submit(submitInfo);
```

### Attente côté CPU (Non bloquante pour les autres frames) :
```cpp
vk::SemaphoreWaitInfo waitInfo{};
waitInfo.semaphoreCount = 1;
waitInfo.pSemaphores = &timelineSemaphore;
waitInfo.pValues = &requiredValue;

// Attend jusqu'à ce que le GPU ait dépassé requiredValue
auto result = device.waitSemaphores(waitInfo, timeoutNs);
```
