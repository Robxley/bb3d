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
* **Format Scène/Assets :** gLTF 2.0 (.gltf pour texte, .glb pour binaire). Standard de l'industrie.  
* **Build System :** CMake.  
* **Portabilité :** Le moteur doit être **OS-Agnostic** (Linux, Windows, Android). Aucune dépendance système directe dans le code de haut niveau.

## **🏗 Architecture & Abstraction**

L'architecture vise une **opacité totale** des technologies sous-jacentes (Vulkan/SDL) pour l'utilisateur du moteur.

1. **Core / Engine (Façade) :** Point d'entrée unique. L'utilisateur instancie Engine, charge des ressources et manipule des objets bb3d. Il ne voit jamais de types Vk\* ou SDL\_\*.  
2. **Renderer (Backend) :** Isole l'implémentation Vulkan. Gère les pipelines PBR, le Shadow Mapping, le post-process et la swapchain.  
3. **Scene Graph :** Structure logique des objets (transformations, hiérarchie) indépendante du rendu.  
4. **Physics World :** Simulation physique découplée du rendu.  
5. **Resources Manager :** Gestionnaire unifié et asynchrone des assets.

## **📜 Classes Fondamentales du Moteur**

Toutes les classes sont dans le namespace bb3d.

### **0\. Point d'Entrée & Gestionnaire**

* **bb3d::Engine :** Façade principale.  
  * Initialise le système de fenêtre, le contexte graphique et le monde physique.  
  * Expose des méthodes de haut niveau : createScene(), loadAsset(), run().  
  * Gère la boucle principale (Update Physics \-\> Update Logic \-\> Render).

### **1\. Composants de Scène (Logique & Environnement)**

Ces classes sont manipulées directement par l'utilisateur du moteur.

* **bb3d::Transform :** Composant essentiel pour positionner les objets.  
  * **Usage GLM Exposé :** Utilise glm::vec3, glm::quat, glm::mat4.  
  * Gère la Position, Rotation, Échelle et la hiérarchie (Parent/Enfant).  
* **bb3d::Camera (Base Abstraite) :**  
  * getViewMatrix(), getProjectionMatrix().  
  * **Frustum Culling :** Doit fournir le frustum pour l'optimisation du rendu.  
  * **Dérivées :** FpsCamera, OrbitCamera.  
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

### **4\. Sérialisation (gLTF)**

* **bb3d::SceneSerializer :** Import/Export gLTF 2.0 via tinygltf.  
  * Support complet des nœuds, meshes, matériaux PBR, lumières, caméras et **Skins/Animations**.

### **5\. Backend (Interne \- Vulkan)**

* **bb3d::VulkanRenderer :**  
  * **Dynamic Shadows :** Cascaded Shadow Maps (CSM) pour le soleil, Omni-directional shadow maps pour les points lights.  
  * **Pipeline :** Forward+ ou Deferred Rendering (à décider selon complexité).

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

### **3\. Style C++**

* **Standard Library (STL) :** Utilisation intensive et prioritaire de la STL.  
  * **Choix Stratégique des Conteneurs (Performance) :**  
    * **std::vector :** Le choix par défaut absolu. La contiguïté mémoire minimise les "Cache Misses".  
    * **std::array :** Obligatoire si la taille est connue à la compilation (stack allocation, zero-overhead).  
    * **std::unordered\_map :** Préférer à std::map pour les lookups (O(1) moyen vs O(log n)). N'utiliser std::map que si l'ordre des clés est vital.  
    * **std::list :** À éviter totalement sauf cas d'école (insertions fréquentes au milieu sans itération). C'est un désastre pour le cache CPU.  
  * **Concurrency :** std::thread (ou std::jthread C++20), std::mutex, std::condition\_variable, std::future.  
  * **Algorithmes :** Utiliser \<algorithm\> et \<numeric\> (std::sort, std::transform, etc.) plutôt que des boucles manuelles complexes.  
* **Modern Parameter Passing (Zero-Copy) :**  
  * **Principe :** Privilégier systématiquement les vues ou mécanismes équivalents évitant la copie.  
  * **Chaînes :** Utiliser std::string\_view au lieu de const std::string&.  
  * **Séquences :** Utiliser std::span\<T\> (C++20) au lieu de const std::vector\<T\>&.  
* **Smart Pointers :** Propriété unique (unique\_ptr) par défaut, partagée (shared\_ptr) pour les ressources (Textures/Meshes).  
* **Naming :** PascalCase (Classes), camelCase (Méthodes), m\_variable (Privé).

### **4\. Performance (Jeu Vidéo)**

* **Hot Path Safety :** Pas d'allocations dans update() ou render().  
* **Data-Oriented Design :** Contiguïté mémoire pour les composants (Transform, RigidBody).  
* **Instancing :** Rendu instancié automatique pour les particules et objets répétés.  
* **Compute Shaders :** Utiliser pour le Culling, les Particules et le Skinning si possible.

## **🔍 Instructions pour l'IA**

1. **Focus Abstraction :** Engine n'inclut jamais \<vulkan/vulkan.h\>.  
2. **PBR :** Les shaders générés doivent être PBR (Physically Based Rendering).  
3. **Maths :** Toujours utiliser GLM.  
4. **Physique :** Propose des interfaces génériques pour la physique (IPhysicsBackend par exemple) pour pouvoir changer de lib (Bullet/Jolt) facilement.  
5. **Animation :** Prévois les structures de données pour le Skinning dès le début (dans Vertex).  
6. **Optimisation :** Vérifie systématiquement l'usage de std::string\_view, std::span et le choix des conteneurs (vector vs list) dans le code généré.