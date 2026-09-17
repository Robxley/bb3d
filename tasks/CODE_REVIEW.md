# Revue de Code — biobazard3d (bb3d)

**Date :** 17 Septembre 2026
**Scope :** `src/bb3d/**`, `include/bb3d/**`, `apps/**`, `CMakeLists.txt`
**Méthode :** Revue statique, lecture manuelle complète des fichiers centraux (Renderer, Engine, VulkanContext, SwapChain, ShadowCascade, Material, Texture, Model, Scene, PhysicsWorld, JobSystem, apps/astro_bazard). Aucune modification du code n'a été effectuée.
**Convention de gravité :** HIGH = bug de correction / crash / fuite ; MEDIUM = comportement incorrect ou perf significative ; LOW = qualité/maintenabilité.

---

## 1. Synthèse exécutive

Le moteur dispose d'un socle Vulkan 1.3 fonctionnel (Dynamic Rendering, VMA, CSM, instancing SSBO, picking GPU). La revue révèle cependant :

- **3 bugs HIGH dans la boucle de rendu** (sémaphore OOB au resize, deadlock de fence si submit échoue, offset d'instances d'ombres incorrect).
- **1 bug HIGH silencieux** : `Material::SetCurrentFrame()` n'est jamais appelé → tous les matériaux n'allouent des descriptor sets que pour la frame 0, créant une course CPU/GPU sur l'UBO avec 3 frames en flight.
- **Plusieurs fuites** : descriptor sets des matériaux (jamais libérés au destructeur), descriptor sets de picking au resize, corps Jolt orphelins à la destruction d'entités.
- **Du code mort** : `Renderer.cpp.bak`, `temp.txt`, bloc d'horizon culling commenté, `m_instanceTransforms`, `getMaterialForTexture`, `m_defaultMaterials`.
- **Performance** : JobSystem en busy-poll 1 kHz/worker, transferts de textures prétendument "async" mais bloquants, rebuild complet des render commands + tri O(n log n) chaque frame, une RenderCommand par particule.
- **Physique** : limite hardcoded de 1024 corps (le README annonce "10k+"), `hardware_concurrency()-1` non clamé, `createRigidBody` sans garde de transform ni check de `CreateBody==nullptr`.

Ce document **complète** l'audit existant `docs/vulkan_audit/RAPPORT_AUDIT_VULKAN_MODERNE.md` (qui couvre Sync2, Timeline Semaphores, Bindless, vk::raii, Push Descriptors, Uber-Vertex, persistance du Pipeline Cache). Les items ci-dessous sont des bugs concrets et du code mort non couverts par cet audit. Les renvois vers l'audit Vulkan sont signalés par `[→ audit]`.

---

## 2. Bugs

### 2.1 Rendu / Vulkan

| ID | Fichier:Ligne | Gravité | Description | Direction de correction |
|----|---------------|---------|-------------|--------------------------|
| B1 | `Renderer.cpp:130, 408-413, 516` | **HIGH** | `m_renderFinishedSemaphores` est dimensionné une fois à `getImageCount()` à l'init et **jamais reconstruit** lors du `recreate()` du swapchain. Le bloc resize (408-413) ne réassigne que `m_imagesInUseFences`. Si `recreate()` produit un nombre d'images différent (bascule fullscreen, DPI), `m_renderFinishedSemaphores[imageIndex]` est un accès hors-limites (UB) et `present()` reçoit une sémaphore périmée. | Reconstruire `m_renderFinishedSemaphores` (détruire l'ancien, créer le nouveau) dans le bloc `m_resizeRequested` alongside `m_imagesInUseFences.assign(...)`. |
| B2 | `Renderer.cpp:433, 517-520` | **HIGH** | `resetFences` (433) est appelé **avant** `submit` (518). Si `submit` lève, le catch (520) ne fait qu'`onResize()` — la fence reste non signalée à jamais. La frame suivante, `waitForFences(..., UINT64_MAX)` (425) bloque la boucle de rendu indéfiniment. | Déplacer `resetFences` après un submit réussi, ou re-signaler la fence dans le catch avant de déclencher le resize. |
| B3 | `Renderer.cpp:924-941` | **HIGH** | Offset d'instances d'ombres incorrect en présence de trous. Quand un non-caster est sauté (929-932), `flushShadowBatch()` est appelé mais `lastMesh` n'est **pas** réinitialisé. Le prochain caster avec le même mesh ne déclenche pas `cmd.mesh != lastMesh`, donc `batchStart` garde l'ancienne valeur (avant le trou) → le draw réutilise la transform du caster précédent / inclut la transform du non-caster. Ombres mal placées ou manquantes. | Sur le chemin de saut, faire `lastMesh = nullptr;` (et positionner `batchStart` au prochain caster) pour démarrer un batch frais après chaque trou. |
| B4 | `Renderer.cpp:1045-1066, 1082-1094` | **MEDIUM** | **[RÉSOLU]** Au resize du picking, `m_pickingDescriptorSets.clear()` libérait le vecteur sans libérer les sets du pool. Corrigé : libération explicite via `freeDescriptorSets` et préservation des sets/buffers sur simple resize. |
| B5 | `Renderer.cpp:450-453, 1071-1094` | **MEDIUM** | **[RÉSOLU]** `createPickingResources()` invoquait `waitIdle()` en milieu de frame (`cb.begin()`). Corrigé : initialisation eager à l'init et redimensionnement d'images dédié dans `m_resizeRequested` hors `cb.begin()`. |
| B6 | `Renderer.cpp:1196-1238` | **MEDIUM** | **[RÉSOLU]** `readEntityIdAt` allouait sur `m_commandPool` et faisait `waitIdle()` global. Corrigé : pool transitoire dédié `m_pickingCommandPool` et synchronisation par fence isolée `m_pickingFence`. |
| B7 | `Renderer.cpp:462-485` | **MEDIUM** | En mode offscreen+éditeur, l'image swapchain est transitionnée vers `eColorAttachmentOptimal` (480) mais `drawScene` ne dessine jamais dedans (cible le RT). `renderUI` utilise ensuite `loadOp::eLoad` sur une image swapchain **non initialisée** → ImGui composé sur du contenu indéfini. | Soit clearer l'image swapchain, soit composites le RT dans l'image viewport ImGui (pas le swapchain) et skipper le dessin swapchain. |
| B8 | `Material.hpp:63, 67` + grep `SetCurrentFrame` | **HIGH** | `Material::SetCurrentFrame()` est déclaré mais **jamais appelé** nulle part (vérifié : aucun appelant dans `src/`, `include/`, `apps/`). `s_currentFrame` reste à 0. Tous les `getDescriptorSet()` indexent donc `m_sets[0]` uniquement ; `m_sets[1]` et `m_sets[2]` restent `nullptr`. Avec `MAX_FRAMES_IN_FLIGHT=3`, le même descriptor set (frame 0) est réutilisé sur les 3 frames, et `m_paramBuffers[0]->update()` est réécrit chaque frame pendant que le GPU peut encore lire la frame précédente → **course CPU/GPU sur l'UBO** et sets nuls pour les frames 1/2. | Appeler `Material::SetCurrentFrame(m_currentFrame)` au début de `Renderer::render()` (ou de `drawScene`), avant toute utilisation de matériau. |
| B9 | `Renderer.cpp:696` | **LOW** | `std::swap(uboData.lights[0], uboData.lights[numLights])` déplace la lumière directionnelle à l'index 0, mais la lumière déjà à l'index 0 se retrouve à `numLights` puis `numLights++` la recompte → compte de lumière potentiellement décalé de 1 envoyé au shader (705). | Collecter la lumière directionnelle à part, sans swap pendant l'itération. |
| B10 | `Renderer.cpp:360` | **LOW** | `getMaterialForTexture` clé le cache par `uintptr_t(texture.get())`. Une texture détruite réutilisée à la même adresse collisionne → mauvais matériau. (Note : cette fonction est de toute façon morte, voir D5.) | Utiliser un id monotonic ou un `weak_ptr` comme clé. |
| B11 | `VulkanContext.cpp:223-240` | **MEDIUM** | `endTransferCommandsAsync` est nommé "Async" mais appelle `m_device.waitForFences()` (235) immédiatement → **synchrone**. La fence retournée est déjà signalée. Le "asynchronous asset loading" du README est donc en fait bloquant. De plus la fence retournée est détruite par l'appelant (`Texture::isReady`/`~Texture`) — cohérent, mais le nom est trompeur. `[→ audit §2.1]` | Rendre le transfert réellement asynchrone (Timeline Semaphore, libération en début de frame suivante). À court terme, renommer pour refléter le comportement. |
| B12 | `SwapChain.cpp:80-90` | **LOW** | `present` avale `eSuboptimalKHR`/`eErrorOutOfDateKHR` (85-86) "handled by caller", mais l'appelant (`Renderer::submitAndPresent` 519) ne vérifie pas le résultat de `present` et ne déclenche pas de resize. Le rendu continue sur un swapchain périmé jusqu'au prochain event de resize. | Propager le out-of-date et déclencher `onResize` côté Renderer. |

### 2.2 Physique

| ID | Fichier:Ligne | Gravité | Description | Direction de correction |
|----|---------------|---------|-------------|--------------------------|
| B13 | `PhysicsWorld.cpp:185` | **MEDIUM** | `static float accumulator = 0.0f;` — static locale partagée entre toutes les instances de `PhysicsWorld` et persistante après `shutdown()/init()`. Non thread-safe si la physique tourne hors-thread ; état périmé après réinit. | Faire de `accumulator` un membre de `Impl`. |
| B14 | `PhysicsWorld.cpp:127` | **MEDIUM** | `(int)std::thread::hardware_concurrency() - 1` non clamé. Si `hardware_concurrency()` retourne 0 (indéterminé) ou 1, cela donne `-1` ou `0` passé à `JPH::JobSystemThreadPool` → UB/crash. (À comparer avec `JobSystem.cpp:16` qui clampe correctement avec `std::max(1u, ...)`.) | `std::max(1, (int)hardware_concurrency() - 1)`. |
| B15 | `PhysicsWorld.cpp:294-298` | **MEDIUM** | `createRigidBody` appelle `entity.get<TransformComponent>()` (298) **sans garde** `has<>()`. Appelé depuis `update()` (195-201) sur **toutes** les entités à `PhysicsComponent` ; toute entité physique sans transform lève/aborte. | Garde `if (!entity.has<TransformComponent>()) return;` (et garantir la transform avant d'ajouter la physique). |
| B16 | `PhysicsWorld.cpp:375-376` | **MEDIUM** | `CreateBody` peut retourner `nullptr` quand la cap de 1024 corps (`Init(1024,...)` ligne 130) est atteinte ; `body->GetID()` déréférence alors null → crash. Le README annonce "10k+ dynamic objects" — la cap 1024 sera atteinte. | Check `body != nullptr` ; relever `maxBodies` dans `Init` à l'échelle attendue (via `EngineConfig`). |
| B17 | `PhysicsWorld.cpp:339-341` | **MEDIUM** | Boucle d'indices mesh-collider `for (i=0; i<indices.size(); i+=3)` lit `indices[i+1]`/`indices[i+2]` sans vérifier `size() % 3 == 0`. Un index buffer malformé/vide cause une lecture OOB. | Assert `indices.size() % 3 == 0` + bound-check ; skipper les meshes à <3 indices. |
| B18 | `PhysicsWorld.cpp:331, 344` | **LOW** | `...Create().Get()` sans vérifier le `Result` ; une géométrie dégénérée asserte à l'intérieur de Jolt. | Vérifier le statut de `Create()` avant `.Get()`. |
| B19 | `PhysicsWorld.cpp:174, 224` | **LOW** | Rotation stockée en angles d'Euler (`glm::eulerAngles`) et reconstruite via `glm::quat(tf.rotation)` à chaque step → gimbal lock / flips d'orientation au round-trip. | Stocker le quaternion directement (ou un quaternion physique séparé). |
| B20 | `PhysicsWorld.cpp:397` | **MEDIUM** | `createCharacterController` appelle `entity.get<TransformComponent>()` (397) sans garde `has<>()`, comme B15. | Ajouter la garde. |

### 2.3 Scène / JobSystem / Ressources

| ID | Fichier:Ligne | Gravité | Description | Direction de correction |
|----|---------------|---------|-------------|--------------------------|
| B21 | `Scene.cpp:154-166` | **MEDIUM** | `destroyEntity` détruit l'entité du registry **sans appeler** `physics().destroyRigidBody()` au préalable. Le corps Jolt (bodyID) reste dans le PhysicsSystem → corps orphelin/fuite. (`Scene::clear()` 435-444 appelle bien `physics().clear()`, mais la destruction individuelle fuit.) | Appeler `destroyRigidBody(entity)` dans `destroyEntity` si l'entité a une `PhysicsComponent` avec bodyID valide. |
| B22 | `Scene.cpp:296` | **MEDIUM** | `glm::normalize(end - start)` — si deux waypoints consécutifs sont égaux, `normalize` d'un vecteur nul produit un NaN qui se propage dans `trans.translation` de façon permanente. | Garde `if (glm::length2(end-start) < epsilon) { avancer waypoint; continue; }`. |
| B23 | `Scene.cpp:282-300` | **LOW** | Le steering par waypoints utilise `dir = normalize(end - start)` (direction du segment) mais déplace depuis `trans.translation`. Une entité déplacée hors-segment avance parallèlement et peut ne jamais atteindre le seuil `dist < 0.1f` → blocage. | `dir = normalize(end - trans.translation)`. |
| B24 | `JobSystem.cpp:104-108` | **MEDIUM** | `m_globalCondition.wait_for(lock, st, 1ms, [&]{ return false; })` — le prédicat est **toujours faux**, donc les workers timeout systématiquement après 1 ms peu importe le `notify_all`. La condition variable ne se réveille jamais sur du vrai travail ; c'est du busy-poll à 1 kHz par worker. Le `notify_all` de `pushInternal` (59) est gaspillé. | Faire que le prédicat vérifie un flag "jobs disponibles" (ou les compteurs de queues) positionné sous `m_globalMutex` avant notify, pour que `wait` retourne sur du vrai travail. |
| B25 | `JobSystem.cpp:148-151` | **LOW** | `wait()` exécute les jobs volés avec un `std::stop_token{}` par défaut — ces jobs croient ne jamais être stoppables. Aussi `static std::atomic<uint32_t> callerIndex` est partagé entre tous les `wait()` concurrents. | Passer un stop token significatif (ou assumer le compromis explicitement) ; rendre l'index de caller non-static. |
| B26 | `ResourceManager.hpp:162-179` + `.cpp:11-18` | **MEDIUM** | `getCache<T>()` retourne une référence à un cache puis relâche `m_registryMutex`. `clearCache()` peut détruire ce cache (via `m_caches.clear()`) pendant qu'un autre thread tient la référence et fait un `getOrLoad` → use-after-free. | Maintenir le lock du registre pendant la load, ou ne jamais détruire les caches (seulement `clear()` leur contenu). |

### 2.4 Logique applicative (AstroBazard)

| ID | Fichier:Ligne | Gravité | Description | Direction de correction |
|----|---------------|---------|-------------|--------------------------|
| B27 | `apps/astro_bazard/main.cpp:77-80, 141-143` | **MEDIUM** | `input.isKeyPressed(Key::R)` (import) et `Key::S` (save) utilisent le test **maintenu** (`isKeyPressed`) au lieu de `isKeyJustPressed`. Maintenir R ou S déclenche l'import/export de la scène **à chaque frame** (réécriture continue du fichier, rechargement en boucle). Backspace (144) utilise correctement `isKeyJustPressed`. | Utiliser `isKeyJustPressed` pour R et S. |
| B28 | `apps/astro_bazard/main.cpp:361-364` | **LOW** | `rand()` non seedé → séquence déterministe identique à chaque exécution. `rand() % 100` introduit un biais de modulo. | `std::srand(std::time(nullptr))` ou préférer `<random>`. |

---

## 3. Code mort / Fichiers morts

| ID | Fichier:Ligne | Gravité | Description | Action |
|----|---------------|---------|-------------|--------|
| D1 | `src/bb3d/render/Renderer.cpp.bak` | **MEDIUM** | Sauvegarde périmée de `Renderer.cpp` (1287 vs 1305 lignes), non compilée (CMake globbe `*.cpp`). C'est une version **buggée** : elle appelle `prepareRenderData` **avant** `updateGlobalUBO`, donc le frustum culling utilisait le frustum de la frame précédente. Le `.cpp` live a corrigé l'ordre. Fichier purement trompeur. | Supprimer `Renderer.cpp.bak`. |
| D2 | `temp.txt` | **LOW** | Snippet orphelin (fin d'en-tête `SmartCameraComponent`, manque le `namespace bb3d {` d'ouverture). Non référencé. | Supprimer `temp.txt`. |
| D3 | `Renderer.hpp:230`, `Renderer.cpp:24,67,717` | **LOW** | `m_instanceTransforms` est `reserve` (24), `clear` (67, 717) mais **jamais écrit ni lu** — l'instance SSBO est rempli directement depuis `m_renderCommands` (830-835). Membre mort. | Supprimer `m_instanceTransforms`. |
| D4 | `Renderer.cpp:357-364` + `Renderer.hpp:138` | **MEDIUM** | `getMaterialForTexture` (privé) n'a **aucun appelant** (vérifié par grep). `m_defaultMaterials` (Renderer.hpp:226) n'est utilisé que par cette fonction morte. Les deux sont morts. | Supprimer `getMaterialForTexture` et `m_defaultMaterials`. |
| D5 | `Scene.cpp:361-399` | **MEDIUM** | Tout le bloc "Horizon Culling" calcule `planetRotation`, itère les caméras, définit `faceDirections` — mais le corps de culling (387-397) est **commenté**. Le bloc ne fait rien yet coûte du CPU par planète par frame (voir P6). | Soit finir/réactiver le culling, soit supprimer le bloc (et `faceDirections`). |
| D6 | `Renderer.cpp:15` | **LOW** | `#include <glm/gtx/string_cast.hpp>` — aucun `glm::to_string` observé dans ce TU. | Supprimer l'include. |
| D7 | `Renderer.cpp:1113,1131,1136,1158,1181` + `1096-1097` | **LOW** | Plusieurs `BB_CORE_INFO` de debug commentés et un bloc `// auto& cb = ...` laissés dans `renderEntityIds`. | Supprimer ou gate derrière un flag de debug. |
| D8 | `tmp/`, `logs/`, `unit_test_logs/` | **LOW** | Répertoires de sortie temporaires (`tmp/planet_*.png`, `take_shot.ps1`, logs). `logs/` et `unit_test_logs/` sont gitignorés, mais `tmp/` ne l'est pas. | Ajouter `tmp/` au `.gitignore` (et nettoyer le contenu committé). |

---

## 4. Performance

| ID | Fichier:Ligne | Gravité | Description | Direction de correction |
|----|---------------|---------|-------------|--------------------------|
| P1 | `Renderer.cpp:715-836` | **MEDIUM** | `prepareRenderData` reconstruit `m_renderCommands` chaque frame (clear + push_back par entité/mesh/particule), puis `std::ranges::sort` O(n log n) sur tout le vecteur, puis une seconde passe pour remplir le SSBO. Pour "10k+ objets", coût CPU significatif par frame. | Liste de commandes persistante ; trier seulement si dirty ; bucketing par (type,material,mesh) à l'insertion. |
| P2 | `Renderer.cpp:776-783` | **MEDIUM** | Les systèmes de particules émettent **une `RenderCommand` par particule vivante**, défaisant l'instancing (chaque particule = entrée de batch séparée). Avec beaucoup de particules, `m_renderCommands` et le coût de tri explosent. | Batcher toutes les particules d'un système en un seul draw instancié via un instance buffer par système. |
| P3 | `Renderer.cpp:824-828` | **LOW** | Le comparateur de tri fait 3 comparaisons (type/material/mesh) par paire. Combiné à P1, c'est chaud. | Précalculer une clé de tri 64-bit (`type<<48 | material<<24 | mesh`) pour une seule comparaison d'entier. |
| P4 | `Renderer.cpp:830-835` | **LOW** | `memcpy` de `glm::mat4` par transform dans une boucle. Les transforms vivent dans `RenderCommand` (non contigus). | Stocker les transforms dans un `std::vector<glm::mat4>` parallèle contigu, puis `memcpy` bulk. |
| P5 | `Renderer.cpp:1196-1227` | **MEDIUM** | `readEntityIdAt` fait un aller-retour GPU complet (`waitIdle`) par lecture de pixel — bloque le thread principal à chaque requête de picking. | Pipeliner le readback (copie ce frame, lecture frame précédente, 1 frame de latence). |
| P6 | `Scene.cpp:363-370` | **LOW** | La lookup de caméra active itère **toutes** les `CameraComponent` pour **chaque** planète (O(planets × cameras)) — et le résultat est inutilisé (voir D5). | Cacher la caméra active une fois par `onUpdate` ; et supprimer le bloc mort. |
| P7 | `JobSystem.cpp:104-108` | **MEDIUM** | Workers se réveillent toutes les 1 ms à cause du prédicat toujours faux (B24) → ~1000 wakeups/sec/worker de polling inutile. | Corriger le prédicat (B24). |
| P8 | `PhysicsWorld.cpp:195-201` | **LOW** | Chaque frame, itère **toutes** les entités à `PhysicsComponent` pour trouver `bodyID == 0xFFFFFFFF` (nouveaux corps). Scan O(n) par frame même quand rien ne change. | Maintenir un set/deque de "création en attente" sur ajout de composant. |
| P9 | `Renderer.cpp:244-251` | **LOW** | `m_layouts[Highlight]` et `m_layouts[Unlit]` appellent tous deux `UnlitMaterial::CreateLayout(dev)` → deux layouts de descripteurs identiques créés et détruits. | Réutiliser un seul layout Unlit partagé pour Highlight. |
| P10 | `Model.cpp:375-381` | **MEDIUM** | Dans la boucle POSITION de `loadGLTF`, `glm::inverse(worldTransform)` et la normale par défaut sont recalculées **par sommet** (même valeur pour tout le primitive), puis écrasées si l'attribut NORMAL existe (385-388). Travail gaspillé. | Calculer `inverse(worldTransform)` une fois par primitive ; ne pas calculer la normale par défaut quand NORMAL existe. |
| P11 | `VulkanContext.cpp:199-201` | **LOW** | `endSingleTimeCommands` fait `m_graphicsQueue.waitIdle()` — stall de queue complet à chaque upload one-shot. `[→ audit §2.1]` | Voir audit : transfers asynchrones / Timeline Semaphores. |

---

## 5. Qualité du code

| ID | Fichier:Ligne | Gravité | Description | Direction de correction |
|----|---------------|---------|-------------|--------------------------|
| Q1 | `Renderer.hpp:149-153,163,179,219,223,263-268` | **MEDIUM** | Nombreux handles bruts (`vk::Image`, `vk::ImageView`, `vk::Sampler`, `vk::DescriptorSetLayout`, `vk::DescriptorPool`, `vk::CommandPool`) nécessitant un `dev.destroyX()` manuel dans le destructeur (84-118). Erreur-prone (cf. B4, B10). `[→ audit §2.1]` | Migration vers `vk::raii::*` pour destruction automatique et exception-safe. |
| Q2 | `Material.cpp` (toutes classes) | **MEDIUM** | Les destructeurs (`~PBRMaterial`, etc.) reset `m_paramBuffers` mais **ne libèrent jamais `m_sets`** du descriptor pool → fuite de descriptor sets à la destruction de matériaux dynamiques. `[→ audit §2.3]` | Libérer `m_sets` dans le destructeur, ou adopter un `DescriptorAllocator` / Push Descriptors. |
| Q3 | `Material.hpp:186-191` | **LOW** | `PlasmaParameters::padding1, padding2` n'ont **pas de valeur par défaut** → bytes non initialisés uploadés dans l'UBO (UB léger). | Initialiser à 0. |
| Q4 | `Material.cpp` (PBR/Unlit/Toon/Plasma/Particle) | **LOW** | Énorme boilerplate dupliqué : `getDescriptorSet`/`updateDescriptorSet` quasi-identiques entre 5 classes. | Extraire une base templated ou un helper commun. |
| Q5 | `Material.cpp:83-86` | **LOW** | `getReadyTexture(m_albedoMap, s_defaultWhite)->getSampler()` et `->getImageView()` appellent `getReadyTexture` deux fois (redondance). | Évaluer `getReadyTexture` une fois dans une variable. |
| Q6 | `Renderer.cpp:267, 879` | **LOW** | Nombre magique `static_cast<MaterialType>(99)` pour la clé du pipeline d'ombres, répété. `[→ audit §2.2]` | Ajouter `MaterialType::Shadow` ou un membre `m_shadowPipeline` dédié. |
| Q7 | `Renderer.hpp:189-212` | **LOW** | `#pragma warning(disable: 4324)` autour de `ShaderLight`/`GlobalUBO` supprime le warning de padding MSVC au lieu d'aligner explicitement. Le layout std140 doit matcher silencieusement. | `alignas(16)` explicite + documenter le contrat std140. |
| Q8 | `Renderer.cpp:416` / `Renderer.hpp:198-211` | **LOW** | `GlobalUBO` est zero-init puis partiellement rempli ; `shadowSplitDepths`/`shadowCascades` ne sont écrits que si les ombres tournent, mais l'UBO est uploadé inconditionnellement → données de cascade périmées d'une frame précédente fuient quand les ombres sont désactivées. | Zero les champs d'ombre explicitement quand `directionalShadows` est faux. |
| Q9 | `Scene.cpp:16-27, 33, 154-166` | **LOW** | `m_EntityNames` autorise les doublons (last-write-wins) ; `destroyEntity` efface par nom même si l'entité détruite n'est pas celle mappée → entrée stale/fausse. | Interdire les doublons ou mapper name → set d'entités. |
| Q10 | `PhysicsWorld.cpp:117-134` | **LOW** | `init()` peut être appelé deux fois (seul `initialized` est gardé, pas la ré-creation). Un second `init()` sans `shutdown()` leak le PhysicsSystem précédent. | `if (m_impl->initialized) shutdown();` en tête de `init()`. |
| Q11 | `PhysicsWorld.cpp:130` | **MEDIUM** | `Init(1024, 0, 1024, 1024, ...)` — limites body/contact/constraint hardcoded ; le README annonce 10k+. Non exposé à la config. | Driver depuis `EngineConfig` / relever les defaults. |
| Q12 | `JobSystem.cpp:6-10, 47-51` | **LOW** | `JobSystem()` vide + `init()` two-phase : si on oublie `init()`, `m_queues` est vide et `pushInternal` (51) divise par `m_queues.size()` = 0. | Init dans le ctor, ou `assert(!m_queues.empty())` dans `pushInternal`. |
| Q13 | `Engine.cpp:27-49` | **LOW** | Singleton `s_Instance` positionné dans le ctor avant `Init()`. Si `Init()` lève, l'objet n'est pas complètement construit, le dtor ne tourne pas, `s_Instance` reste pendant. | Positionner `s_Instance` après `Init()` réussi, ou wrapper dans `CreateScope` qui reset sur exception. |
| Q14 | `VulkanContext.cpp:126` | **LOW** | `vk::PhysicalDeviceFeatures deviceFeatures{}` active **zéro** feature (pas de `samplerAnisotropy`, `fillModeNonSolid`, etc.). L'anisotropie est indisponible. | Activer les features nécessaires explicitement (via `PhysicalDeviceVulkan13Features` / `Vulkan12Features`). `[→ audit §2.1]` |
| Q15 | `Renderer.cpp:1116` | **LOW** | `0xFFFFFFFF` sentinel "no hit" non documenté et dupliqué avec `readEntityIdAt`. | Constante nommée `kPickingNoEntity`. |

---

## 6. Lien avec l'audit Vulkan existant

Le fichier `docs/vulkan_audit/RAPPORT_AUDIT_VULKAN_MODERNE.md` couvre déjà les sujets de modernisation Vulkan suivants (non répétés ici en détail) :

- `pipelineBarrier` → `pipelineBarrier2` / Sync2
- Fences/Semaphores binaires → Timeline Semaphores
- Transferts bloquants → staging asynchrone (B11, P11 ci-dessus rejoignent cet axe)
- Handles bruts → `vk::raii` (Q1)
- Descriptors par matériau → Push Descriptors + Bindless (Q2)
- Uber-Vertex 88 octets → streams séparés
- Pipeline Cache non persisté → sauvegarde sur disque
- Extended Dynamic State
- Debug labels Vulkan / Tracy GPU zones

La présente revue se concentre sur les **bugs concrets** et le **code mort** que l'audit de modernisation n'aborde pas. Les deux documents sont complémentaires : l'audit Vulkan décrit la cible architecturale, celui-ci liste les défauts à corriger dans l'immédiat.

---

## 7. Plan d'amélioration priorisé (pour planning futur)

### Phase 0 — Corrections critiques (sécurité d'exécution)
Objectif : éliminer crashes, deadlocks et courses de la boucle de rendu.
1. **B8** — Appeler `Material::SetCurrentFrame(m_currentFrame)` au début de `Renderer::render()`. (1 ligne, impact énorme.)
2. **B1** — Reconstruire `m_renderFinishedSemaphores` au resize du swapchain.
3. **B2** — Déplacer `resetFences` après un submit réussi.
4. **B3** — Réinitialiser `lastMesh = nullptr` sur le saut de non-caster dans `renderShadows`.
5. **B15/B20/B16** — Gardes de transform + clamp `hardware_concurrency` + check `CreateBody != nullptr` dans `PhysicsWorld`.
6. **B21** — Appeler `destroyRigidBody` dans `Scene::destroyEntity`.

### Phase 1 — Fuites et ressources
1. **Q2** — Libérer `m_sets` dans les destructeurs de matériaux (ou adopter un `DescriptorAllocator`).
2. **B4** — Libérer les descriptor sets de picking avant `clear()` au resize.
3. **D1/D2** — Supprimer `Renderer.cpp.bak` et `temp.txt`.
4. **D4** — Supprimer `getMaterialForTexture` + `m_defaultMaterials` + `m_instanceTransforms`.
5. **D5** — Finir ou supprimer le bloc d'horizon culling commenté.

### Phase 2 — Stabilité physique & scène
1. **B13** — Membre `accumulator` au lieu de static.
2. **B14** — Clamp du thread count Jolt.
3. **B17/B18** — Validation des index mesh et des `Create().Get()`.
4. **B22/B23** — Waypoints : garde zero-length + direction depuis la position.
5. **Q11** — Exposer les limites Jolt à l'`EngineConfig`.
6. **B19** — Stockage quaternion pour éviter le gimbal lock.

### Phase 3 — Concurrence & perf CPU
1. **B24/P7** — Corriger le prédicat du `wait_for` du JobSystem (supprime le busy-poll 1 kHz).
2. **B26** — Lock du registre ResourceManager pendant la load.
3. **P1/P2/P3** — Liste de commandes persistante, clé de tri 64-bit, batch unique par système de particules.
4. **P5** — Readback de picking pipelined.
5. **P10** — `inverse(worldTransform)` une fois par primitive dans `loadGLTF`.

### Phase 4 — Qualité & nettoyage
1. **B27** — `isKeyJustPressed` pour S/R dans AstroBazard.
2. **Q3** — Initialiser `PlasmaParameters::padding`.
3. **Q4/Q5** — Dédupliquer le boilerplate matériaux.
4. **Q6/Q7/Q8/Q15** — Constantes nommées, alignement explicite, zero des champs d'ombre.
5. **D6/D7/D8** — Includes morts, logs commentés, `.gitignore` pour `tmp/`.
6. **Q13** — Safety du singleton Engine.

### Phase 5 — Modernisation Vulkan (selon `docs/vulkan_audit/`)
Rejoindre la feuille de route en 4 phases de l'audit Vulkan existant (Sync2 → Timeline Semaphores → Push Descriptors/Bindless → streams de sommets). Les corrections des phases 0-2 ci-dessus sont des prérequis qui stabilisent le backend avant la modernisation.

---

## 8. Notes méthodologiques

- Revue statique uniquement ; aucun test n'a été exécuté dans cet environnement. Les bugs marqués HIGH sont déduits de la lecture du code et de la vérification par grep des appels (ex. `SetCurrentFrame`, `getMaterialForTexture`, `m_instanceTransforms`).
- `grep`/`ripgrep` n'étaient pas disponibles via l'outil dédié ; les recherches de références ont été faites via `bash grep` (confirmées pour D3, D4, B8).
- Le fichier `Renderer.cpp.bak` a été comparé au `.cpp` live : la différence clé est l'ordre `prepareRenderData`/`updateGlobalUBO` (le `.bak` est buggy, le `.cpp` est corrigé).
- Les fichiers `src/bb3d/render/stb_image_write.cpp` (2 lignes) et `src/bb3d/render/stb_image_write.cpp` sont des TU stb légitimes (implémentation de `stb_image_write` pour les captures d'écran), **pas** du code mort.
- Périmètre non couvert en profondeur : `ImGuiLayer.cpp` (1154 lignes, survolé), `SceneSerializer.cpp`, `PickingSystem.cpp`, `InputManager.cpp`, `ComputePipeline.cpp`, `RenderTarget.cpp`, `Buffer.cpp`, shaders GLSL. Une revue de suivi de ces fichiers est recommandée.
