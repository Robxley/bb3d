# Document de Conception : Timeline Semaphores & Transferts Réellement Asynchrones (`B11`, `P11`)

- **Auteur :** @Antigravity
- **Date :** 2026-09-18
- **Statut :** PROPOSED (Approche A validée par l'utilisateur)
- **Branche :** `feat/timeline-semaphores-transfers`
- **Fiche Tâche Associée :** `tasks/active/TASK-TIMELINE-SEMAPHORES-TRANSFERS.md`

---

## 1. Contexte & Problématique

### 1.1 Contexte Technique
Vulkan 1.2 introduit les **Timeline Semaphores** (`VK_KHR_timeline_semaphore` désormais Core 1.2/1.3/1.4). Contrairement aux sémaphores binaires qui oscillent uniquement entre état signalé et non-signalé, un Timeline Semaphore manipule un compteur entier 64-bit strictement monotone (`uint64_t`).

Dans le moteur `bb3d` :
- `VulkanContext` active déjà `feat12.timelineSemaphore = VK_TRUE` (et Vulkan 1.4 core features).
- Le hardware sous-jacent (RTX 3070 Laptop, Vulkan 1.4.0) supporte nativement ces fonctionnalités.
- Cependant, le moteur n'instancie ni n'utilise encore **aucun Timeline Semaphore**.

### 1.2 Anomalies Identifiées dans le Catalogue (`CODE_REVIEW.md`)
1. **`B11` (`VulkanContext.cpp:406`) : Faux Asynchronisme des Transferts**
   `endTransferCommandsAsync()` porte le suffixe "Async" mais contient :
   ```cpp
   (void)m_device.waitForFences(fence, true, std::numeric_limits<uint64_t>::max());
   ```
   L'appel bloque le thread CPU appelant jusqu'à complétion GPU. Tout chargement de texture ("asynchronous asset loading") gèle donc le thread de la boucle de jeu.
2. **`P11` (`VulkanContext.cpp:372`) : Stall Queue Global sur SingleTime Commands**
   `endSingleTimeCommands()` appelle `m_graphicsQueue.waitIdle()`, paralysant l'ensemble de la file de rendu lors des copies staging vers vertex/index buffers.
3. **Surcoût & Risque de Fuite de Fences**
   Chaque texture alloue une `vk::Fence` Vulkan brute, la détruit manuellement dans `isReady()` ou dans son destructeur, augmentant la fragmentation des allocations d'objets de synchronisation.

---

## 2. Objectifs & Exigences

1. **Zéro Attente Bloquante CPU (`waitIdle` / `waitForFences`)** lors des transferts de textures et d'assets.
2. **Timeline Semaphore Unique de Transfert** dans `VulkanContext` partagé par la queue de transfert (`m_transferQueue`).
3. **Interrogation Non-Bloquante dans `Texture::isReady()`** via `m_device.getSemaphoreCounterValue(...)`.
4. **Recyclage Automatique des Command Buffers de Transfert** sans fuite mémoire ni destruction prématurée pendant que le GPU travaille.
5. **Préparation & Compatibilité pour l'Approche B** : Exposition des accesseurs et handles pour permettre ultérieurement au `Renderer` d'attendre directement le Timeline Semaphore sur la file graphique via `vk::SubmitInfo2` / `vk::SemaphoreSubmitInfo`.

---

## 3. Architecture Technique Détaillée

### 3.1 `VulkanContext`

#### A. Nouveaux Membres
```cpp
// Timeline Semaphore pour la file de transfert
vk::Semaphore m_transferTimelineSemaphore;
std::atomic<uint64_t> m_transferTimelineValue{0};

// Structure de recyclage différé des command buffers de transfert
struct PendingTransfer {
    vk::CommandBuffer commandBuffer;
    uint64_t timelineValue;
};
std::mutex m_transferMutex;
std::vector<PendingTransfer> m_pendingTransfers;
```

#### B. Cycle de Vie
- **Création (`initVulkan`)** :
  ```cpp
  vk::SemaphoreTypeCreateInfo typeInfo(vk::SemaphoreType::eTimeline, 0);
  vk::SemaphoreCreateInfo semInfo{};
  semInfo.pNext = &typeInfo;
  m_transferTimelineSemaphore = m_device.createSemaphore(semInfo);
  ```
- **Destruction (`cleanup`)** :
  Attente idle, nettoyage de `m_pendingTransfers`, puis `m_device.destroySemaphore(m_transferTimelineSemaphore)`.

#### C. Méthodes Publiques & API Moderne
```cpp
// Récupère la valeur cible incrémentée à la soumission
uint64_t endTransferCommandsAsync(vk::CommandBuffer commandBuffer);

// Interrogation non-bloquante de la dernière valeur complétée par le GPU
[[nodiscard]] uint64_t getCompletedTransferTimelineValue() const;

// Récupère la dernière valeur soumise
[[nodiscard]] uint64_t getTransferTimelineValue() const noexcept;

// Handle du sémaphore pour synchronisation inter-queues (Approche B)
[[nodiscard]] vk::Semaphore getTransferTimelineSemaphore() const noexcept;

// Attente CPU explicite (uniquement pour destructeurs ou tests unitaires)
void waitTransferTimeline(uint64_t value, uint64_t timeoutNs = std::numeric_limits<uint64_t>::max()) const;

// Nettoyage non-bloquant des command buffers terminés
void pollTransferCompletions();
```

#### D. Implémentation d'`endTransferCommandsAsync`
```cpp
uint64_t VulkanContext::endTransferCommandsAsync(vk::CommandBuffer commandBuffer) {
    commandBuffer.end();

    std::lock_guard<std::mutex> lock(m_transferMutex);
    pollTransferCompletionsLocked();

    uint64_t signalValue = ++m_transferTimelineValue;

    vk::TimelineSemaphoreSubmitInfo timelineInfo{};
    timelineInfo.signalSemaphoreValueCount = 1;
    timelineInfo.pSignalSemaphoreValues = &signalValue;

    vk::SubmitInfo submitInfo{};
    submitInfo.pNext = &timelineInfo;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_transferTimelineSemaphore;

    m_transferQueue.submit(submitInfo, nullptr); // ZÉRO FENCE, ZÉRO ATTENTE BLOQUANTE !

    m_pendingTransfers.push_back({ commandBuffer, signalValue });
    return signalValue;
}
```

---

### 3.2 `Texture`

#### A. Remplacement de `m_uploadFence`
Dans `Texture.hpp` :
```cpp
// Remplacement de vk::Fence par la valeur timeline cible
uint64_t m_uploadTimelineValue = 0;
bool m_ready = false;
Scope<Buffer> m_stagingBuffer;
```

#### B. Implémentation de `isReady()`
```cpp
bool Texture::isReady() {
    if (m_ready) return true;
    if (m_uploadTimelineValue == 0) {
        m_ready = true;
        return true;
    }

    uint64_t completed = m_context.getCompletedTransferTimelineValue();
    if (completed >= m_uploadTimelineValue) {
        m_stagingBuffer.reset(); // Libération propre de la mémoire CPU hôte
        m_uploadTimelineValue = 0;
        m_ready = true;
        BB_CORE_TRACE("Texture: Upload complete (Timeline: {} >= Target: {}).", completed, m_uploadTimelineValue);
        return true;
    }
    return false;
}
```

#### C. Destructeur `~Texture()`
Si l'objet est détruit avant la complétion du transfert (ex: sortie précipitée de l'application) :
```cpp
if (m_uploadTimelineValue > 0 && !m_ready) {
    m_context.waitTransferTimeline(m_uploadTimelineValue);
    m_stagingBuffer.reset();
}
```
Plus aucune destruction de fence (`dev.destroyFence`) requise.

---

### 3.3 Préparation pour l'Approche B (Frame Timeline & Synchronisation Queue-to-Queue)

Dans l'Approche B ultérieure :
1. `Renderer` pourra introduire `m_frameTimelineSemaphore` pour suivre l'avancement des frames CPU/GPU sans fences binaires.
2. Lors de l'utilisation de textures dynamiquement chargées, `Renderer::render()` pourra insérer une dépendance dans `SubmitInfo2` pour attendre `m_transferTimelineSemaphore` à l'étape `vk::PipelineStageFlagBits2::eFragmentShader` sans bloquer le CPU.

---

## 4. Stratégie TDD & Vérification

### 4.1 Nouveau Test Unitaire Dédié : `tests/unit_test_33_timeline_transfers.cpp`
1. **Test 1 : Création & Propriétés du Timeline Semaphore**
   - Vérifier que `getTransferTimelineSemaphore()` est valide.
   - Vérifier la valeur initiale = 0.
2. **Test 2 : Incrément Monotone Asynchrone**
   - Lancer plusieurs commandes de transfert séquentielles et parallèles.
   - Vérifier que `endTransferCommandsAsync` retourne des valeurs strictement croissantes ($1, 2, 3...$).
   - Vérifier que `getCompletedTransferTimelineValue()` finit par atteindre ces valeurs.
3. **Test 3 : Cycle de Vie Texture Réellement Asynchrone**
   - Charger une texture de test en mémoire.
   - Constater immédiatement après le constructeur que `isReady()` peut être interrogé sans blocage CPU.
   - Attendre la complétion via `waitTransferTimeline` et vérifier que `isReady()` passe à `true` avec libération du staging buffer.
4. **Test 4 : Recyclage des Command Buffers**
   - Soumettre une série de transferts et vérifier que `m_pendingTransfers` est recyclé sans fuite ni crash de validation layer.

---

## 5. Critères de Réussite
- Zéro appel à `waitForFences` bloquant dans les transferts de textures.
- Zéro warning compilateur MSVC `/W4`.
- 100% de succès sur `ctest` avec les validation layers Vulkan actives.
- Revue de code formelle par Mistral Vibe CLI signée `[APPROVED]`.
