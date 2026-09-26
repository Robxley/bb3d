# [TÂCHE-PIPELINE-CACHE] : Persistance du Pipeline Cache Vulkan (`vk::PipelineCache`)

- **Statut :** READY FOR CODE REVIEW
- **Auteur / Implémenteur :** Antigravity
- **Reviewer(s) :** bb3d-reviewer (glm-5.3) / Antigravity
- **Branche Git :** `feat/persistent-pipeline-cache`
- **Date de création :** 2026-09-26
- **Design Doc Associé :** [`tasks/active/2026-09-26-pipeline-cache-persistant-design.md`](2026-09-26-pipeline-cache-persistant-design.md)

---

## 1. Contexte & Objectif

Dans un jeu vidéo haute performance, la compilation des shaders au vol par le driver Vulkan crée des micro-saccades (hitches/stutters) préjudiciables à la fluidité.
L'objectif de cette tâche est de sérialiser l'objet `vk::PipelineCache` sur disque (`assets/cache/pipelines.bin`) et de le recharger au démarrage de l'application.

### Critères d'Acceptation :
1. **Validation stricte de l'en-tête (32 octets) :** Vérification de `headerSize`, `headerVersion`, `vendorID`, `deviceID` et `pipelineCacheUUID` pour éliminer tout risque de crash lors d'un changement de GPU ou mise à jour de pilote.
2. **Sauvegarde atomique :** Écriture via un fichier temporaire (`.tmp`) puis remplacement atomique.
3. **Opacité de l'API Publique :** Aucune fuite de types Vulkan dans les interfaces de haut niveau (`Config.hpp`, `Engine.hpp`).
4. **Zéro-Allocation dans le Hot-Path :** Gestion I/O confinée à l'initialisation et à la fermeture.
5. **Couverture de tests unitaire complète :** Test TDD `tests/unit_test_35_pipeline_cache.cpp` validant la validation du header, la sérialisation, et le rechargement.

---

## 2. Découpage en Tâches Atomiques (Approche TDD)

### Tâche 1 : Configuration & Déclaration API
- **Fichiers modifiés :** `include/bb3d/core/Config.hpp`, `include/bb3d/render/VulkanContext.hpp`
- Ajout de `enablePipelineCache` (défaut `true`) et `pipelineCachePath` (`"assets/cache/pipelines.bin"`) dans `GraphicsConfig`.
- Déclaration des méthodes `isPipelineCacheValid`, `savePipelineCache`, `loadPipelineCache`, `setPipelineCachePath`, `getPipelineCachePath` dans `VulkanContext`.

### Tâche 2 : Test TDD Initial (RED)
- **Fichiers créés :** `tests/unit_test_35_pipeline_cache.cpp`
- **Fichier modifié :** `CMakeLists.txt` (enregistrement du test unitaire 35)
- **Étape 1 (Test) :** Rédaction des tests :
  - Validation du header (taille < 32 octets, mauvais UUID, mauvais vendor, cache valide).
  - Sauvegarde d'un cache avec pipeline réel sur disque temporaire.
  - Rechargement du cache dans un second contexte.
- **Étape 2 (Vérification échec) :** `cmake --build build && ctest -R unit_test_35` (RED validé).

### Tâche 3 : Implémentation du Pipeline Cache dans `VulkanContext` (GREEN)
- **Fichiers modifiés :** `src/bb3d/render/VulkanContext.cpp`, `src/bb3d/core/Engine.cpp`
- Implémentation de `isPipelineCacheValid()` avec comparaison exacte des propriétés physiques (`props.vendorID`, `props.deviceID`, `props.pipelineCacheUUID`).
- Implémentation de `savePipelineCache()` avec écriture atomique (.tmp -> rename) et création des dossiers parents.
- Implémentation de `loadPipelineCache()` avec vérification de validité.
- Intégration dans `initLogicalDevice` (chargement transparent du fichier si valide) et `cleanup()` (sauvegarde automatique).
- Intégration dans `Engine::Init()` pour transmettre le chemin depuis `m_Config.graphics`.
- **Étape 4 (Vérification succès) :** `unit_test_35_pipeline_cache` PASS (1.15s) et 7/7 tests graphiques passés.

### Tâche 4 : Revue de Code Croisée & Double Check
- Revue systématique par reviewer.
- Validation des checkpoints.
- Signature `APPROVED` et archivage.

---

## 3. Grille de Revue & Checkpoints

### 🛠️ Checkpoints de l'Implémenteur (Avant soumission en revue)
- [x] **Double Check Bug (si correctif) :** N/A (Nouvelle fonctionnalité).
- [x] **TDD & Tests :** Les nouveaux tests et les tests unitaires existants passent (`ctest`).
- [x] **Standards C++ (`cpp-pro`) :**
  - [x] Zéro allocation dynamique dans le *Hot Path* (`render()` / `update()`).
  - [x] `std::span` et `std::string_view` utilisés pour le passage de paramètres (Zero-Copy).
  - [x] Initialisation désignée C++20 (`Type{.field = val}`).
  - [x] `[[nodiscard]]` présent sur les accesseurs et fonctions critiques.
  - [x] Code, commentaires, logs et documentation Doxygen rédigés en **anglais**.
- [x] **Standards Vulkan (`vulkan-cpp`) :**
  - [x] Synchronisation moderne : `pipelineBarrier2` avec `vk::DependencyInfo`.
  - [x] Zéro attente bloquante CPU (`waitIdle()`) pour les uploads.
  - [x] Validation du header Vulkan (UUID/Vendor/Device) avant de passer les données à `vk::PipelineCacheCreateInfo`.
  - [x] Dynamic Rendering sans RenderPass legacy.
- [x] **Qualité du Build :** Zéro warning compilateur (`/W4` sous MSVC).
- [x] **Commits :** Commits atomiques et messages de commit clairs (`feat:`, `fix:`).

---

### 🔍 Checkpoints des Reviewers (Revue de Code Systématique & Approbation)
- [ ] **Architecture & Opacité (`AGENTS.md`) :**
  - [ ] Les types Vulkan (`vk::*`) restent 100% opaques vis-à-vis du code utilisateur/scene.
  - [ ] Respect de la séparation CPU/GPU (pas de transfert inutile par frame).
  - [ ] Multi-streams sommets respecté.
- [ ] **Sécurité & Robustesse :**
  - [ ] Vérifications lourdes correctement isolées sous `#if defined(BB3D_DEBUG)`.
  - [ ] Gestion des cas limites (fichier absent, dossier manquant, fichier corrompu, mise à jour de pilote).
- [ ] **Validation GPU & Profiling :**
  - [ ] Validation Layers Khronos sans erreur ni warning.
  - [ ] Absence de micro-saccades ou de blocage I/O in-game.
- [ ] **Décision Reviewer :**
  - [ ] **APPROVED** (Prêt pour la fusion)
  - [ ] **CHANGES REQUESTED** (Voir commentaires ci-dessous)
- [ ] **Historique :** 1 entrée compacte consignée dans `tasks/HISTORY.md`.

---

## 4. Journal des Échanges & Retours de Revue
- *2026-09-26* - **@Antigravity** : Création de la tâche et rédaction du plan d'architecture TDD.
