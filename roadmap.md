# **🗺️ Roadmap de Développement \- biobazard3d**

Ce document détaille le plan de développement séquentiel du moteur **biobazard3d**.  
**Philosophie :** Chaque étape (Milestone) est validée par un exécutable de test unitaire autonome (Sandbox) qui prouve le fonctionnement du module de manière isolée avant l'intégration.

## **📅 Phase 1 : Infrastructure & Core (Hybrid C++20)**

*Objectif : Mettre en place les fondations "OS-Agnostic" et le système de build hybride.*

### **Étape 1.0 : Structure du Projet & Initialisation CMake**

* **Objectifs Techniques :**  
  * Créer la hiérarchie de dossiers standard : src/bb3d, include/bb3d, tests/, assets/.  
  * Créer le CMakeLists.txt racine définissant le standard **C++20** et les flags de compilation stricts.  
  * **Stratégie de Mise à Jour :** Ce fichier central sera enrichi à chaque étape suivante pour inclure les nouveaux fichiers sources (.cpp/.ixx) et lier les nouvelles librairies externes (SDL, Vulkan, Jolt) au fur et à mesure de leur introduction.  
* **Validation :**  
  * La commande cmake \-S . \-B build configure le projet sans erreur.  
  * Génération d'une solution vide prête à accueillir le code.

### **Étape 1.1 : Logging & Profiling**

* **Objectifs Techniques :**  
  * Intégrer les dépendances dans le CMake : spdlog (Logging), Tracy (Profiling), nlohmann\_json (Config).  
  * Définir les alias de types fondamentaux (bb3d::Ref, bb3d::Scope) et les macros de debug (BB\_PROFILE\_SCOPE).  
  * Implémenter le système de log initial.  
* **Validation (unit\_test\_00\_infrastructure.cpp) :**  
  * Compile en C++20.  
  * Affiche "Hello Engine" via spdlog.  
  * Génère une trace visible dans le serveur Tracy.  
  * Vérifie que les macros de profiling disparaissent en build Release.

### **Étape 1.2 : Fenêtrage & Configuration**

* **Objectifs Techniques :**  
  * Implémenter bb3d::Config (Sérialisation JSON vers struct C++).  
  * Créer l'abstraction bb3d::Window. Cacher totalement SDL\_Window\* en interne.  
  * **Architecture :** Préparer le switch SDL2/SDL3 via des flags de compilation (bien que SDL3 soit prioritaire).  
* **Validation (unit\_test\_01\_window.cpp) :**  
  * Charge engine\_config.json (Résolution, Titre).  
  * Ouvre une fenêtre noire.  
  * La fenêtre se ferme proprement sur Echap ou Croix (Event Loop basique).

### **Étape 1.3 : Input System**

* **Objectifs Techniques :**  
  * Créer une classe `bb3d::Input` (Singleton ou Service).
  * Mapper les événements SDL3 (Clavier/Souris/Manette) vers des codes abstraits `bb3d::Key` et `bb3d::Mouse`.
  * Méthodes de polling : `IsKeyPressed()`, `IsMouseButtonPressed()`, `GetMousePosition()`.
* **Validation (unit\_test\_01\_window.cpp - Mise à jour) :**  
  * Déplacer un carré ou afficher des logs lors de l'appui sur Z/Q/S/D.
  * Fermer la fenêtre via `Input::IsKeyPressed(Key::Escape)`.

## **📅 Phase 2 : Backend Vulkan (Initialisation)**

*Objectif : Initialiser le contexte graphique Vulkan 1.3 "Modern" (Dynamic Rendering).*

### **Étape 2.1 : Instance & Device**

* **Objectifs Techniques :**  
  * Créer VulkanInstance avec Validation Layers actives en Debug.  
  * Sélectionner le GPU discret (Physical Device).  
  * Initialiser VulkanMemoryAllocator (VMA).  
* **Validation (unit\_test\_02\_vulkan\_init.cpp) :**  
  * Initialise Vulkan sans erreur.  
  * Affiche le nom du GPU dédié dans la console.  
  * Aucune erreur de Validation Layer à la destruction (Zero Leaks).

### **Étape 2.2 : SwapChain & Présentation**

* **Objectifs Techniques :**  
  * Créer la Surface SDL/Vulkan.  
  * Implémenter la SwapChain (Triple Buffering si V-Sync).  
  * Gérer les ImageViews pour le rendu.  
  * **Synchronisation :** Créer les Semaphores (ImageAvailable, RenderFinished) et Fences (InFlight) pour gérer la synchronisation CPU/GPU.
* **Validation (unit\_test\_03\_swapchain.cpp) :**  
  * Initialise la Swapchain.  
  * Gère le redimensionnement de la fenêtre (Recreation de la Swapchain détectée dans les logs).

## **📅 Phase 3 : Le Premier Triangle (Pipeline Graphique)**

*Objectif : Rendu graphique minimaliste mais architecturalement correct.*

### **Étape 3.1 : Pipeline & Dynamic Rendering**

* **Objectifs Techniques :**  
  * **Build System :** Ajouter une commande CMake pour compiler automatiquement les Shaders (.vert/.frag) en SPIR-V (.spv) via `glslc`.
  * Créer le GraphicsPipeline sans VkRenderPass (Usage de **Dynamic Rendering** Vulkan 1.3).  
  * Enregistrer les Command Buffers.  
* **Validation (unit\_test\_04\_hello\_triangle.cpp) :**  
  * Affiche un triangle coloré (positions hardcodées dans le Vertex Shader).  
  * Valide l'absence de RenderPass legacy.

### **Étape 3.2 : Vertex Buffers & VMA**

* **Objectifs Techniques :**  
  * Abstraction bb3d::Buffer.  
  * Transfert CPU \-\> GPU via Staging Buffer (VMA).  
  * Définition des Layouts de sommets (VertexPos, VertexStatic).  
* **Validation (unit\_test\_05\_vertex\_buffer.cpp) :**  
  * Affiche un quad (2 triangles) dont les sommets sont définis dans un std::vector C++.

## **📅 Phase 4 : Textures & Maths 3D**

*Objectif : Transition vers la 3D et gestion des ressources.*

### **Étape 4.1 : Uniform Buffers (UBO) & GLM**

* **Objectifs Techniques :**  
  * Intégrer **GLM**.  
  * Implémenter les DescriptorSets pour lier les matrices MVP.  
  * Créer une Camera basique (LookAt).  
* **Validation (unit\_test\_06\_rotating\_cube.cpp) :**  
  * Affiche un cube 3D en rotation.  
  * La perspective est correcte (pas de déformation au resize).

### **Étape 4.2 : Système de Textures & Caching**

* **Objectifs Techniques :**  
  * Charger des images (ktx2 ou stb\_image).  
  * Abstraction bb3d::Texture gérée par VMA.  
  * **Caching :** Le chargement doit vérifier si l'image est déjà en mémoire.  
* **Validation (unit\_test\_07\_texture\_cube.cpp) :**  
  * Le cube est texturé.  
  * Charger la texture 2 fois ne provoque qu'une seule allocation VRAM (Log de confirmation).

## **📅 Phase 5 : Architecture Moteur (Scene & Assets)**

*Objectif : Sortir du code "Vulkan brut" pour l'API Fluent bb3d::.*

### **Étape 5.1 : Core Systems (Jobs & Events)**

* **Objectifs Techniques :**  
  * JobSystem : Thread Pool pour tâches background.  
  * EventBus : Système Pub/Sub typé.  
* **Validation (unit\_test\_08\_core\_systems.cpp) :**  
  * Lance 100 jobs parallèles incrémentant un std::atomic.  
  * Déclenche un PlayerDeathEvent reçu par deux systèmes distincts.

### **Étape 5.2 : Resource Manager & gLTF**

* **Objectifs Techniques :**  
  * Intégration tinygltf.  
  * Chargement asynchrone via JobSystem.  
  * Conversion gLTF \-\> bb3d::Model / bb3d::Mesh.  
* **Validation (unit\_test\_09\_load\_gltf.cpp) :**  
  * Charge un asset complexe (ex: SciFiHelmet.gltf) sans geler le thread principal.  
  * L'objet apparaît une fois chargé.

### **Étape 5.3 : ECS & Scene Graph**

* **Objectifs Techniques :**  
  * Implémenter Entity, Component et la hiérarchie Transform.  
  * Implémenter l'API Fluent (entity.add\<Mesh\>().at(...)).  
* **Validation (unit\_test\_10\_ecs\_scene.cpp) :**  
  * Instancie 100 entités via l'API Fluent.  
  * Vérifie que bouger un Parent déplace bien les Enfants.

### **Étape 5.4 : Sérialisation**

* **Objectifs Techniques :**  
  * Implémenter serialize() / deserialize() pour les composants.  
  * Export/Import JSON complet de la scène.  
* **Validation (unit\_test\_11\_serialization.cpp) :**  
  * Sauvegarde une scène modifiée \-\> Reset Moteur \-\> Charge JSON \-\> État identique.

## **📅 Phase 6 : Fonctionnalités Avancées**

*Objectif : Gameplay et Rendu PBR.*

### **Étape 6.1 : Éclairage PBR**

* **Objectifs Techniques :**  
  * Shaders PBR (Metallic/Roughness Workflow).  
  * Gestion Skybox (HDR) et Light (Directional \+ Shadows).  
* **Validation (unit\_test\_12\_pbr\_environment.cpp) :**  
  * Rendu réaliste de sphères avec différents niveaux de roughness.  
  * Ombres portées dynamiques (Shadow Mapping).

### **Étape 6.2 : Physique & Audio (La Règle de Synchro)**

* **Objectifs Techniques :**  
  * Intégrer **Jolt Physics** et **miniaudio**.  
  * **Sync Rule :** Implémenter la logique où PhysicsWorld écrase Transform pendant l'Update.  
* **Validation (unit\_test\_13\_physics\_audio.cpp) :**  
  * Une caisse tombe (gravité Jolt).  
  * Le Transform visuel suit la caisse sans "jitter".  
  * Un son 3D est joué à l'impact.

### **Étape 6.3 : Terrain & FX**

* **Objectifs Techniques :**  
  * Terrain via Heightmap.  
  * ParticleSystem (CPU ou Compute Shader simple).  
* **Validation (unit\_test\_14\_terrain\_fx.cpp) :**  
  * Rendu d'un terrain vallonné avec fumée.

### **Étape 6.4 : Outils & Hot Reload**

* **Objectifs Techniques :**  
  * Intégrer **ImGui**.  
  * HotReloader : Surveiller les fichiers Shaders.  
* **Validation (unit\_test\_15\_tools\_hotreload.cpp) :**  
  * Modification d'un shader frag sur le disque \= Changement de couleur instantané sans redémarrage.