# **Context & Instructions : Projet biobazard3d (C++/SDL/Vulkan)**

Ce document définit les standards techniques, l'architecture et les contraintes pour le développement du moteur de jeu **biobazard3d**. À consulter avant toute génération de code ou proposition d'architecture.

## **🛠 Stack Technique**

* **Nom du Projet :** biobazard3d  
* **Namespace :** bb3d  
* **Langage :** C++20 (ou supérieur) \- Utilisation intensive des smart pointers (std::unique\_ptr, std::shared\_ptr) et de la RAII.  
* **Windowing/Input :** SDL3 (prioritaire) ou SDL2.  
* **Graphics API :** Vulkan 1.3+ (Focus sur la performance et la gestion explicite de la mémoire).  
* **Gestion Mémoire :** Vulkan Memory Allocator (VMA).  
* **Maths :** GLM (OpenGL Mathematics).  
* **Physique :** Abstraction nécessaire (Jolt Physics, Bullet ou PhysX en backend).  
* **Audio :** miniaudio (Header-only, performant) ou OpenAL Soft.  
* **Format Scène/Assets :** gLTF 2.0 (.gltf pour texte, .glb pour binaire). Standard de l'industrie.  
* **Configuration/Sauvegarde :** JSON (via nlohmann/json ou équivalent léger).  
* **Logging :** spdlog.  
* **Profiling :** Tracy Profiler.  
* **Build System :** CMake.  
* **Portabilité :** Le moteur doit être **OS-Agnostic** (Linux, Windows, Android). Aucune dépendance système directe dans le code de haut niveau.

## **🏗 Architecture & Abstraction**

L'architecture vise une **opacité totale** des technologies sous-jacentes (Vulkan/SDL) pour l'utilisateur du moteur.

1. **Core / Engine (Façade) :** Point d'entrée unique. L'utilisateur instancie Engine, charge des ressources et manipule des objets bb3d. Il ne voit jamais de types Vk\* ou SDL\_\*.  
2. **Renderer (Backend) :** Isole l'implémentation Vulkan. Gère les pipelines PBR, le Shadow Mapping, le post-process et la swapchain.  
3. **Scene Graph :** Structure logique des objets (transformations, hiérarchie) indépendante du rendu.  
4. **Physics World :** Simulation physique découplée du rendu.  
5. **Audio System :** Gestion spatiale du son.  
6. **Resources Manager :** Gestionnaire unifié et asynchrone des assets.

## **📜 Classes Fondamentales du Moteur**

Toutes les classes sont dans le namespace bb3d.

### **0\. Point d'Entrée & Gestionnaire (Core)**

* **bb3d::Engine :** Façade principale.  
  * Initialise le système de fenêtre, le contexte graphique, l'audio et le monde physique.  
  * Expose des méthodes de haut niveau : createScene(), loadAsset(), run().  
  * Gère la boucle principale (Update Physics \-\> Update Logic \-\> Render).  
* **bb3d::Config :** Gestionnaire de configuration global.  
  * **Fichier :** Charge engine\_config.json au démarrage.  
  * **Modularité (Activation à la demande) :**  
    * Le moteur doit suivre le principe "Pay for what you use".  
    * Intégrer des flags explicites pour activer les sous-systèmes lourds : enableAudio, enablePhysics, enableJobSystem.  
    * **Comportement :** Si un module est désactivé (via json ou code), ses ressources ne sont pas allouées, ses threads ne sont pas lancés, et son overhead CPU/Mémoire doit être nul.  
  * **Paramètres :** Résolution par défaut, V-Sync, FPS Max, Threads Max pour le JobSystem, Max Particles, Debug Level.  
  * **Fallback :** Valeurs par défaut codées en dur si le fichier est absent (modules activés par défaut pour la facilité d'utilisation, sauf indication contraire).  
* **bb3d::JobSystem :** Gestion du multithreading.  
  * **Architecture :** Thread Pool créé au démarrage (taille définie dans Config).  
  * **Usage :** Traitement parallèle pour le Culling, les Animations, la Physique et le chargement d'Assets.  
  * **API :** JobSystem::execute(\[\]{ ... }) ou JobSystem::dispatch(count, granularity, func).  
* **bb3d::EventBus :** Système de communication découplé.  
  * **Pattern :** Publish/Subscribe.  
  * **Usage :** Permet aux systèmes (UI, Audio, Gameplay) de communiquer sans dépendances directes (ex: PlayerDiedEvent).  
* **bb3d::HotReloader (Dev Tools) :**  
  * **Fonction :** Surveille les changements de fichiers sur le disque (shaders, textures, config json) en mode Debug.  
  * **Action :** Déclenche le rechargement automatique des pipelines Vulkan ou des assets sans redémarrer le moteur.

### **1\. Composants de Scène (Logique & Environnement)**

Ces classes sont manipulées directement par l'utilisateur du moteur. **Elles doivent être sérialisables.**

* **bb3d::Transform :** Composant essentiel pour positionner les objets.  
  * **Usage GLM Exposé :** Utilise glm::vec3, glm::quat, glm::mat4.  
  * Gère la Position, Rotation, Échelle et la hiérarchie (Parent/Enfant).  
* **bb3d::Camera (Base Abstraite) :**  
  * getViewMatrix(), getProjectionMatrix().  
  * **Frustum Culling :** Doit fournir le frustum pour l'optimisation du rendu.  
  * **Dérivées :** FpsCamera, OrbitCamera.  
* **bb3d::AudioSource & bb3d::AudioListener :**  
  * **Source :** Émetteur de son 3D attaché à une Entité/Transform. Propriétés : Volume, Pitch, Loop, SpatialBlend.  
  * **Listener :** L'oreille (généralement sur la Caméra).  
* **bb3d::Light :**  
  * Types : Directional (Soleil), Point, Spot.  
  * **Shadows :** Propriété castShadows (bool). Gère les matrices de vue pour la génération de Shadow Maps (CSM pour directionnelle).  
* **bb3d::Skybox & bb3d::Fog :**  
  * **Skybox :** CubeMap HDR pour l'éclairage ambiant (IBL \- Image Based Lighting) et le fond.  
  * **Fog :** Brouillard exponentiel ou volumétrique pour la profondeur (Distance, Couleur, Densité).  
* **bb3d::Terrain :**  
  * **LOD (Level of Detail) :** Gestion dynamique du maillage basée sur la distance caméra (CDLOD ou Tesselation shaders).  
  * **Heightmap :** Chargement depuis textures 16-bit.  
* **bb3d::ParticleSystem :**  
  * Système de particules GPU (Compute Shaders préférés).  
  * Paramètres : Émetteur, Durée de vie, Vitesse, Gravité, Texture.

### **2\. Physique & Collision (Abstraction)**

* **bb3d::PhysicsWorld :** Gère la simulation (Gravity, Step simulation).  
* **bb3d::RigidBody :**  
  * Types : Static, Dynamic, Kinematic.  
  * Propriétés : Masse, Friction, Restitution.  
* **bb3d::Collider :** Formes de collision (Box, Sphere, Capsule, MeshCollider).  
* **bb3d::CharacterController :** Gestion physique spécifique pour les déplacements de personnages (évite de glisser, monte les escaliers).

### **3\. Ressources & Rendu (PBR & Animation)**

* **bb3d::Material (Workflow PBR Metallic-Roughness) :**  
  * **Maps :** Albedo, Normal, Metallic, Roughness, Ambient Occlusion (AO), Emissive.  
  * **Pipeline :** Utilise des shaders PBR conformes à l'équation de rendu physique (Cook-Torrance BRDF).  
* **bb3d::Model / bb3d::Mesh :**  
  * **Skeletal Animation :** Support des armatures (Bones) et des animations (Skinning).  
  * **Animator :** Contrôleur pour jouer/mixer des animations (playAnimation("Walk")).  
* **bb3d::Texture :** Chargement asynchrone, Mipmapping, support formats compressés (KTX2).  
* **bb3d::RenderTarget :** Pour le rendu Off-screen (Shadow Maps, Post-Process).

### **4\. UI Runtime & Sérialisation**

* **bb3d::UISystem (Runtime) :**  
  * **But :** Affichage de l'interface utilisateur du jeu (HUD, Menu Pause, Barre de vie).  
  * **Technique :** Rendu batché de quads texturés (VertexUI) ou intégration de RmlUi / libRocket si besoin d'HTML/CSS like. Séparé de ImGui (Outils).  
* **bb3d::SceneSerializer :**  
  * **Import/Export Assets :** gLTF 2.0 via tinygltf pour la géométrie.  
  * **Sauvegarde État (JSON) :** Gère la sérialisation de la hiérarchie de la scène et des composants via nlohmann/json.

### **5\. Backend (Interne \- Vulkan & SDL)**

* **bb3d::VulkanRenderer :**  
  * **Dynamic Shadows :** Cascaded Shadow Maps (CSM) pour le soleil, Omni-directional shadow maps pour les points lights.  
  * **Pipeline :** Forward+ ou Deferred Rendering (à décider selon complexité).  
* **bb3d::InputManager :**  
  * **Action Mapping :** Abstraction des entrées physiques. Ne pas utiliser Key::Space directement dans le jeu, mais Action::Jump.  
  * **Configuration :** Permettre le remapping des touches via fichier de config.

## **📦 Modélisation 3D & Vertex (Modulaire)**

Pour optimiser la bande passante mémoire (Bandwidth) et le Vertex Fetch, le moteur supporte plusieurs layouts de sommets. L'utilisation d'une structure "Uber-Vertex" unique est proscrite pour la production.

* **Système Flexible :**  
  * Implémenter un mécanisme (Traits ou Templates) pour générer automatiquement les VkVertexInputAttributeDescription et VkVertexInputBindingDescription.  
* **Formats Standards Suggérés :**  
  * **VertexPos :** Uniquement position (pour Shadow Maps, Z-Prepass, Collisions).  
  * **VertexStatic (Standard PBR) :** position, normal, uv, tangent (calculé si besoin).  
  * **VertexAnim (Skinned Mesh) :** VertexStatic \+ boneIds (ivec4), weights (vec4).  
  * **VertexUI / Vertex2D :** position (vec2), color, uv.

## **🔮 Roadmap : Outils & Éditeur**

Pour les futures versions, l'outillage sera séparé du Runtime.

* **bb3d::EngineEditor (Hérite de Engine) :**  
  * **Interface :** Utilisation de **ImGui** (avec backend Vulkan/SDL3).  
  * **Fonctionnalités :**  
    * Inspecteur de scène (Arbre des entités).  
    * Éditeur de propriétés (Transform, Material, Physics).  
    * Gizmos de manipulation (Translation/Rotation/Scale) dans la vue 3D.  
  * **Architecture :** L'éditeur s'injecte comme une surcouche de rendu (Overlay) sur le moteur standard.

## **📜 Règles de Codage & Standards**

### **1\. Abstraction & Portabilité**

* **API Publique :** Aucun type Vulkan (Vk...) ou SDL dans les headers publics.  
* **Physique :** Ne pas exposer directement les types du moteur physique sous-jacent (ex: btRigidBody).

### **2\. Gestion de Vulkan (Interne)**

* **VMA :** Usage exclusif pour l'allocation mémoire.  
* **Synchronisation :** Gestion explicite et documentée.

### **3\. Style C++ (Modern C++ & Modules)**

* **Structure de Fichiers :**  
  * **Règle Absolue :** Une classe majeure par fichier.  
  * Séparez clairement les interfaces (.hpp ou .ixx) des implémentations (.cpp).  
* **Modern C++ Features (C++20/23) :**  
  * **Modules :** Privilégier l'utilisation des **Modules C++** (import, export) pour les nouvelles classes afin d'améliorer l'encapsulation et les temps de build. Garder une compatibilité header pour les libs externes non-modulaires.  
  * **Concepts :** Utiliser les **Concepts** pour contraindre les paramètres de template (template\<typename T\> requires std::integral\<T\>) au lieu de SFINAE.  
  * **Ranges :** Utiliser std::ranges et les vues (std::views) pour la manipulation de collections et les algorithmes (ex: filtrage, transformation) au lieu des boucles brutes.  
  * **Coroutines :** Utiliser les coroutines (co\_await, co\_return) pour les tâches asynchrones (chargement d'assets, scripts de comportement) plutôt que des callbacks complexes.  
* **Standard Library (STL) :** Utilisation intensive et prioritaire de la STL.  
  * **Choix Stratégique des Conteneurs (Performance) :**  
    * **std::vector :** Le choix par défaut absolu. La contiguïté mémoire minimise les "Cache Misses".  
    * **std::array :** Obligatoire si la taille est connue à la compilation (stack allocation, zero-overhead).  
    * **std::unordered\_map :** Préférer à std::map pour les lookups (O(1) moyen vs O(log n)). N'utiliser std::map que si l'ordre des clés est vital.  
    * **std::list :** À éviter totalement sauf cas d'école.  
  * **Concurrency :** Privilégier bb3d::JobSystem. Utiliser std::mutex si nécessaire.  
  * **Algorithmes :** Utiliser \<algorithm\> et \<numeric\> (std::sort, std::transform, etc.) plutôt que des boucles manuelles complexes.  
* **Modern Parameter Passing (Zero-Copy) :**  
  * **Chaînes :** Utiliser std::string\_view au lieu de const std::string&.  
  * **Séquences :** Utiliser std::span\<T\> (C++20) au lieu de const std::vector\<T\>&.  
* **Smart Pointers :** Propriété unique (unique\_ptr) par défaut, partagée (shared\_ptr) pour les ressources.  
* **Naming :** PascalCase (Classes), camelCase (Méthodes), m\_variable (Privé).  
* **Developer Experience (DX) \- Defaults :**  
  * **Règle :** Tous les objets de haut niveau (Components, Resources) doivent être générés avec des **paramètres par défaut fonctionnels**.  
  * **Objectif :** Simplifier la tâche de l'utilisateur. Une instantiation sans argument (ex: entity.add\<Light\>()) doit produire un résultat immédiatement valide et visible (ex: Lumière blanche, intensité 1.0, portée standard) sans nécessiter de configuration complexe obligatoire.

### **4\. Sérialisation & Réflexion (Sauvegarde)**

* **Interface de Sérialisation (Mandatoire) :**  
  * **Exigence :** Toutes les classes définissant l'état du jeu (notamment les **Components**, **Resources** et la **Config**) doivent être sérialisables.  
  * **Implémentation :** Chaque classe doit fournir des méthodes serialize(json& j) et deserialize(const json& j) (ou compatible nlohmann/json) ou s'intégrer dans un système de réflexion statique interne.  
  * **Objectif :** Permettre à Engine::exportScene() de générer un fichier JSON complet représentant l'état exact de la scène (position des entités, paramètres des lumières, chemins des assets) sans perte d'information.

### **5\. Performance (Jeu Vidéo)**

* **Hot Path Safety :** Pas d'allocations dans update() ou render().  
* **Data-Oriented Design :** Contiguïté mémoire pour les composants (Transform, RigidBody).  
* **Instancing :** Rendu instancié automatique pour les particules et objets répétés.  
* **Compute Shaders :** Utiliser pour le Culling, les Particules et le Skinning si possible.

### **6\. Debugging, Logging & Tests (Outils Internes)**

* **Système de Log & Trace (spdlog) :**  
  * **Bibliothèque :** Utiliser **spdlog**.  
  * **Architecture :** Wrapper l'initialisation dans bb3d::Log. Loggers séparés "CORE" et "CLIENT".  
  * **Macros :** Utiliser BB\_CORE\_INFO(...), BB\_ERROR(...).  
  * **Compile-time Strip :** Configurer SPDLOG\_ACTIVE\_LEVEL pour supprimer les logs en Release.  
* **Profiling Visuel (Tracy) :**  
  * **Outil :** **Tracy Profiler**. C'est le standard pour le profiling Frame/GPU/Memory en C++.  
  * **Macros :** Définir des macros BB\_PROFILE\_FRAME(name) et BB\_PROFILE\_SCOPE(name) qui appellent Tracy.  
  * **Stripping :** Ces macros doivent être définies comme vides (\#define BB\_PROFILE\_SCOPE(name)) si le flag de profiling n'est pas activé (Build Release).  
* **Tests Unitaires (Zéro Dépendance) :**  
  * **Philosophie :** Pas de frameworks externes lourds. Système minimaliste interne (BB\_TEST\_CASE).

## **🔍 Instructions pour l'IA**

1. **Focus Abstraction :** Engine n'inclut jamais \<vulkan/vulkan.h\>.  
2. **PBR :** Les shaders générés doivent être PBR.  
3. **Maths :** Toujours utiliser GLM.  
4. **Physique :** Interface générique (IPhysicsBackend).  
5. **Animation :** Structures Skinning dans Vertex.  
6. **Optimisation :** std::string\_view, std::span, vector vs list.  
7. **Config & Log :** Implémenter le chargement de engine\_config.json et les macros spdlog/Tracy.  
8. **Architecture :** Intégrer JobSystem et EventBus dans les propositions d'architecture Core.  
9. **Modern C++ :** Utiliser les **Modules**, **Concepts**, **Ranges** et **Coroutines** dans le code généré.  
10. **Sérialisation :** Assurer que tout code de composant généré inclut les hooks de sérialisation JSON pour l'export.  
11. **Defaults :** Générer systématiquement des valeurs par défaut valides pour tous les composants.  
12. **Modularité :** N'initialiser les systèmes (Audio, Physique, Jobs) que s'ils sont explicitement activés dans Config.

### **7\. Exemple Complet (Kitchen Sink Demo)**

Voici un fichier main.cpp illustrant l'usage de toutes les fonctionnalités majeures (Core, Audio, Physique, FX, Input) via l'API Fluent.