# [TASK-VULKAN-1.4-INIT] : Initialisation Vulkan 1.4 via StructureChain & Features Core

- **Statut :** APPROVED (Code Review Validée)
- **Auteur / Implémenteur :** @Antigravity
- **Reviewer(s) :** @bb3d-reviewer (Vibe CLI), @dev
- **Branche Git :** `feat/vulkan-1.4-structurechain`
- **Date de création :** 2026-09-18
- **Date de révision d'architecture :** 2026-09-18
- **Date de revue de code :** 2026-09-18
- **Document de Conception :** `tasks/archive/2026-09-18-vulkan-1.4-structurechain-design.md`

---

## 1. Contexte & Objectif

Dans le cadre du démarrage du **Jalon 2 (Socle Vulkan 1.3/1.4 Moderne & Synchronisation)**, cette première tâche établit les fondations graphiques modernes en modernisant l'initialisation de l'instance et du device Vulkan dans `VulkanContext`.

**Objectifs validés lors de la revue d'architecture conjointe avec l'agent Vibe (`bb3d-reviewer`) :**
1. Déclarer `VK_API_VERSION_1_4` dans `vk::ApplicationInfo` et aligner VMA (`allocatorInfo.vulkanApiVersion`).
2. Négocier l'API version effective : `min(instanceVersion, physicalDeviceProps.apiVersion)`.
3. Interroger les fonctionnalités supportées par le matériel via `m_physicalDevice.getFeatures2(&queryChain)` avant toute activation (interdiction de lever `VK_ERROR_FEATURE_NOT_PRESENT`).
4. Remplacer le chaînage manuel par pointeurs bruts `void* pNext` par `vk::StructureChain` type-safe pour la création du `vk::Device`.
5. Activer de manière propre et robuste :
   - **Features Requises (Hard Fail si absentes) :** `dynamicRendering`, `synchronization2`, `timelineSemaphore`.
   - **Features Optionnelles (Activées si supportées) :** `samplerAnisotropy`, `pushDescriptor`, `dynamicRenderingLocalRead`, `maintenance5`, `maintenance6`, `maintenance4`, `descriptorIndexing`, `runtimeDescriptorArray`, `descriptorBindingPartiallyBound`, `descriptorBindingVariableDescriptorCount`.
6. Chemin de fallback Vulkan 1.3 complet et typé via `StructureChain` 1.3.
7. Pas de redondance d'extensions : `VK_KHR_swapchain` unique sur 1.4.
8. Defer de `bufferDeviceAddress` au Jalon 3 (GPU-driven culling).
9. Validation TDD via enrichissement de `unit_test_02_vulkan_init`.

---

## 2. Découpage en Tâches Atomiques (Approche TDD)

### Tâche 1 : Négociation de Version & StructureChain dans VulkanContext
- **Fichiers modifiés :** `include/bb3d/render/VulkanContext.hpp`, `src/bb3d/render/VulkanContext.cpp`
- **Test unitaire associé :** `tests/unit_test_02_vulkan_init.cpp`
- **Étape 1 (Test) :** Enrichir `unit_test_02_vulkan_init.cpp` pour vérifier :
  - `context.getApiVersion() >= VK_API_VERSION_1_3`
  - Les features obligatoires `dynamicRendering`, `synchronization2`, `timelineSemaphore` sont bien actives.
  - Logger si Vulkan 1.4 et `pushDescriptor` sont actifs.
- **Étape 2 (Vérification échec) :** Compiler et vérifier le statut du test avant les modifications (RED vérifié).
- **Étape 3 (Code minimal) :**
  - Mettre à jour `VulkanContext::init` pour déclarer `VK_API_VERSION_1_4`.
  - Implémenter la requête `getFeatures2` préalable.
  - Construire la `vk::StructureChain` (1.4 ou 1.3) avec validation des prérequis.
  - Aligner VMA avec la version négociée.
- **Étape 4 (Vérification succès) :** `ctest -R unit_test_02_vulkan_init` PASS, validation zéro warning.
- **Étape 5 (Commit) :** `feat(render): initialize Vulkan 1.4 via vk::StructureChain and core features`

---

## 3. Grille de Revue & Checkpoints

### 🛡️ Checkpoints de l'Implémenteur (Avant soumission en revue)
- [x] **Détection de Version :** Négociation propre entre Vulkan 1.4 et repli 1.3 selon `apiVersion` du GPU.
- [x] **Query Préalable :** `getFeatures2` appelé avant `createDevice` pour valider le support matériel des bits.
- [x] **Type-Safety Vulkan-Hpp :** Utilisation stricte de `vk::StructureChain` sans aucun `pNext` manipulé manuellement.
- [x] **Features Core Activées :** `synchronization2`, `dynamicRendering`, `timelineSemaphore`, `pushDescriptor` (si dispo), `samplerAnisotropy`.
- [x] **VMA Configuré :** `allocatorInfo.vulkanApiVersion` aligné sur la version négociée.
- [x] **TDD & Tests :** `unit_test_02_vulkan_init` enrichi et passant à 100%.
- [x] **Qualité du Build :** Zéro warning compilateur, logs en anglais technique.

---

### 🔍 Checkpoints des Reviewers (Revue d'Architecture & Revue de Code)
- [x] **Architecture :** L'approche par `vk::StructureChain` est validée et extensible pour Sync2, Timeline Semaphores et Push Descriptors.
- [x] **Gestion des Pilotes / Portabilité :** Le mécanisme de fallback Vulkan 1.3 et la query data-driven protègent les environnements anciens et iGPUs.
- [x] **Recommandations Spécifiques :** Extensions redondantes éliminées, `bufferDeviceAddress` différé au Jalon 3, `dynamicRenderingLocalRead` inclus.
- [x] **Revue de Code C++20 / Vulkan-Hpp :** Zero pointeur brut `pNext`, structure chain type-safe, zéro allocation dans l'init critique, gestion RAII et Dynamic Dispatcher VMA propre.
- [x] **Validation des Tests :** Suite de tests unitaires 100% passante (12/12 tests PASS dont `unit_test_02_vulkan_init`).
- [x] **Décision Reviewer :** [x] APPROVED | [ ] CHANGES REQUESTED

---

## 4. Journal des Échanges & Retours de Revue
- *2026-09-18* - **@Antigravity** : Proposition initiale du plan de migration Vulkan 1.4 via StructureChain.
- *2026-09-18* - **@bb3d-reviewer** : Revue d'architecture Vibe (`[CHANGES REQUESTED]`). 9 recommandations formulées :
  1. Ajouter `dynamicRenderingLocalRead` dans le bloc 1.4.
  2. Aligner `descriptorBindingVariableDescriptorCount` en 1.2.
  3. Obligation de query préalable `getFeatures2` pour éviter tout crash `FEATURE_NOT_PRESENT`.
  4. Chaîne concrète complète pour le fallback 1.3.
  5. Éliminer les extensions KHR redondantes avec le Core 1.4.
  6. Différer `bufferDeviceAddress` au Jalon 3.
  7. Négocier la version contre `min(instance, physicalDevice)`.
  8. Spécifier la stratégie de test dans `unit_test_02`.
  9. Documenter la dette technique existante (`waitIdle` des transferts) pour le chantier B11.
- *2026-09-18* - **@Antigravity** : Intégration complète des 9 points dans le document de conception et la fiche de tâche. Plan d'architecture validé conjointement et prêt pour approbation utilisateur.
- *2026-09-18* - **@Robxley (Utilisateur)** : Validation formelle du plan ("Je valide ce plan").
- *2026-09-18* - **@Antigravity** : Implémentation TDD complète (RED -> GREEN), tests unitaires validés (12/12 PASS).
- *2026-09-18* - **@bb3d-reviewer** : Revue de code formelle Vibe CLI (`[APPROVED]`, code 0 en 47s). Deux observations mineures relevées :
  - Harmonisation de la mention de `samplerAnisotropy` en optionnelle dans les documents de spécification.
  - Ajout des drapeaux `descriptorBindingPartiallyBound` et `descriptorBindingVariableDescriptorCount` dans le fallback Vulkan 1.3 pour alignement bindless complet.
- *2026-09-18* - **@Antigravity** : Double-check effectué, observations appliquées dans `VulkanContext.cpp` et dans la documentation. Validation finale de la tâche.
