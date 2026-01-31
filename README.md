# biobazard3d 🚀

**biobazard3d** est un moteur de jeu 3D moderne écrit en C++20, utilisant Vulkan comme backend graphique. Le projet suit une approche de développement séquentiel avec validation par tests unitaires à chaque étape.

## 📖 À propos

biobazard3d est conçu pour être un moteur de jeu complet avec les caractéristiques suivantes :

- **Architecture moderne** : C++20, Vulkan 1.3, ECS (Entity Component System)
- **Rendu avancé** : PBR (Physically Based Rendering), gestion des matériaux, ombres dynamiques
- **Système de physique** : Intégration avec Jolt Physics
- **Gestion des ressources** : Chargement asynchrone, cache des assets
- **Système de scène** : Sérialisation/deserialization JSON, hiérarchie d'entités
- **Outils de développement** : Profiling avec Tracy, logging avec spdlog

## 🔧 Technologies et dépendances

Le projet utilise les technologies et bibliothèques suivantes, toutes gérées automatiquement via CMake FetchContent :

### Dépendances principales

| Technologie | Version | Description | Statut |
|-------------|---------|-------------|--------|
| **C++** | 20 | Standard C++20 avec compilation stricte | ✅ Requis |
| **Vulkan** | 1.3+ | Backend graphique moderne | ✅ Requis |
| **SDL3** | 3.4.0 | Gestion des fenêtres et des entrées | ✅ Intégré |
| **spdlog** | 1.17.0 | Système de logging performant | ✅ Intégré |
| **nlohmann_json** | 3.12.0 | Parsing et génération JSON | ✅ Intégré |
| **Tracy** | 0.13.1 | Profiling et optimisation | ✅ Intégré |
| **VulkanMemoryAllocator** | 3.3.0 | Gestion de la mémoire Vulkan | ✅ Intégré |
| **GLM** | 1.0.3 | Bibliothèque mathématique pour l'infographie | ✅ Intégré |
| **stb** | master | Chargement d'images | ✅ Intégré |
| **fastgltf** | 0.8.0 | Chargement de modèles 3D au format glTF | ✅ Intégré |
| **tinyobjloader** | release | Chargement de modèles 3D au format OBJ | ✅ Intégré |
| **EnTT** | 3.13.2 | Bibliothèque ECS (Entity Component System) | ✅ Intégré |

### Dépendances optionnelles

| Technologie | Version | Description | Statut |
|-------------|---------|-------------|--------|
| **Jolt Physics** | - | Moteur de physique 3D | ⏳ Intégration en cours |
| **miniaudio** | - | Bibliothèque audio légère | ⏳ Prévu |
| **ImGui** | - | Interface utilisateur immédiate | ⏳ Prévu |

### Outils de développement

| Outil | Version | Description |
|-------|---------|-------------|
| **CMake** | 3.20+ | Système de build multiplateforme |
| **glslc** | Vulkan SDK | Compilateur de shaders GLSL → SPIR-V |
| **Doxygen** | - | Génération de documentation (optionnel) |

## 📦 Gestion des dépendances

Toutes les dépendances sont automatiquement téléchargées et configurées via CMake FetchContent. Aucune installation manuelle n'est nécessaire à l'exception de :

- **Vulkan SDK** : Doit être installé manuellement et disponible dans le PATH
- **CMake 3.20+** : Requis pour le système de build
- **Compilateur C++20** : MSVC, GCC ou Clang avec support complet du standard

Le fichier `CMakeLists.txt` contient la configuration complète pour toutes les dépendances.

## 🛠️ Configuration requise

- **Système d'exploitation** : Windows (testé), Linux (théoriquement supporté)
- **Compilateur** : MSVC (Windows) ou GCC/Clang (Linux) avec support C++20
- **Vulkan SDK** : Version 1.3 ou supérieure
- **CMake** : Version 3.20 ou supérieure
- **Git** : Pour la gestion des dépendances

## 🚀 Installation et compilation

### Prérequis

Avant de commencer, assurez-vous d'avoir installé :

- **Vulkan SDK 1.3+** : [Téléchargement Vulkan SDK](https://vulkan.lunarg.com/)
- **CMake 3.20+** : [Téléchargement CMake](https://cmake.org/download/)
- **Compilateur C++20** : MSVC 2022+, GCC 11+, ou Clang 14+
- **Git** : Pour le clonage du dépôt et la gestion des sous-modules

### 1. Cloner le dépôt

```bash
git clone https://github.com/votre-utilisateur/biobazard3d.git
cd biobazard3d
```

### 2. Configurer l'environnement

#### Sur Windows

1. **Installer Vulkan SDK** et ajouter `VULKAN_SDK` à vos variables d'environnement
2. **Ajouter Vulkan au PATH** : Ajoutez `%VULKAN_SDK%\Bin` à votre variable PATH
3. **Vérifier l'installation** :

```bash
where glslc
vulkaninfo | findstr "apiVersion"
```

#### Sur Linux

```bash
# Installer les dépendances système (Ubuntu/Debian)
sudo apt update
sudo apt install vulkan-tools libvulkan-dev vulkan-validationlayers-dev-spirv

# Vérifier l'installation
vulkaninfo | grep apiVersion
```

### 3. Configurer et compiler

#### Configuration CMake commune

```bash
mkdir build
cd build
```

#### Sur Windows (Visual Studio 2022)

```bash
cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --config Debug --parallel 8
```

#### Sur Windows (MinGW)

```bash
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --config Debug --parallel 8
```

#### Sur Linux (Makefiles)

```bash
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --config Debug --parallel 8
```

#### Sur Linux (Ninja - recommandé)

```bash
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --config Debug
```

### 4. Options de compilation avancées

Vous pouvez personnaliser la compilation avec ces options CMake :

```bash
# Désactiver le profiling Tracy (par défaut activé)
cmake -DBB3D_ENABLE_PROFILING=OFF ..

# Build Release optimisé
cmake -DCMAKE_BUILD_TYPE=Release ..

# Spécifier un compilateur spécifique
cmake -DCMAKE_CXX_COMPILER=clang++ ..
```

### 5. Exécuter les tests et démonstrations

#### Exécuter tous les tests

```bash
ctest -C Debug -V
```

#### Exécuter un test spécifique

```bash
# Exemple : exécuter le test des matériaux
./tests/Debug/unit_test_16_materials

# Ou sur Linux
./tests/unit_test_16_materials
```

#### Liste des démonstrations disponibles

| Test | Description | Commande |
|------|-------------|----------|
| `unit_test_04_hello_triangle` | Premier triangle Vulkan | `./tests/unit_test_04_hello_triangle` |
| `unit_test_06_rotating_cube` | Cube 3D avec éclairage | `./tests/unit_test_06_rotating_cube` |
| `unit_test_10_obj_loader` | Chargement de modèles OBJ | `./tests/unit_test_10_obj_loader` |
| `unit_test_11_gltf_loader` | Modèles glTF complexes | `./tests/unit_test_11_gltf_loader` |
| `unit_test_14_interactive_cameras` | Caméras interactives | `./tests/unit_test_14_interactive_cameras` |
| `unit_test_16_materials` | Matériaux PBR/Unlit/Toon | `./tests/unit_test_16_materials` |

### 6. Dépannage

#### Problèmes courants

**Erreur : `glslc` introuvable**
```
Solution : Ajoutez Vulkan SDK Bin à votre PATH ou spécifiez manuellement :
cmake -DGLSLC_EXECUTABLE="C:/VulkanSDK/1.3.268.0/Bin/glslc.exe" ..
```

**Erreur : Vulkan SDK non trouvé**
```
Solution : Installez Vulkan SDK et définissez VULKAN_SDK :
set VULKAN_SDK=C:\VulkanSDK\1.3.268.0
```

**Erreur : C++20 non supporté**
```
Solution : Mettez à jour votre compilateur ou activez le support C++20 :
- MSVC : Utilisez Visual Studio 2022+
- GCC : Ajoutez -std=c++20 aux flags
- Clang : Ajoutez -std=c++20 aux flags
```

**Problèmes de performances**
```
Solution : Essayez un build Release et désactivez les validation layers :
cmake -DCMAKE_BUILD_TYPE=Release ..
```

## 📁 Structure du projet

```
bb3d/
├── include/bb3d/          # En-têtes publics
│   ├── audio/             # Système audio
│   ├── core/              # Noyau du moteur
│   ├── input/             # Gestion des entrées
│   ├── physics/           # Système de physique
│   ├── render/            # Rendu graphique
│   ├── resource/          # Gestion des ressources
│   └── scene/             # Système de scène et ECS
│
├── src/bb3d/             # Implémentations
│   ├── audio/
│   ├── core/
│   ├── input/
│   ├── physics/
│   ├── render/
│   ├── resource/
│   └── scene/
│
├── tests/                # Tests unitaires
│   ├── unit_test_00_infrastructure.cpp
│   ├── unit_test_01_window.cpp
│   ├── ...
│   └── unit_test_16_materials.cpp
│
├── assets/               # Ressources (modèles, textures, shaders)
│   ├── models/
│   ├── shaders/
│   └── textures/
│
├── CMakeLists.txt        # Configuration CMake
├── engine_config.json    # Configuration du moteur
└── roadmap.md            # Feuille de route du développement
```

## 🎮 Fonctionnalités principales

### Moteur de rendu Vulkan avancé

- **Pipeline graphique moderne** : Vulkan 1.3 avec Dynamic Rendering et **GPU Instancing** (SSBO) pour des performances optimales
- **Batching Automatique** : Réduction drastique des Draw Calls par regroupement des objets identiques
- **Système de matériaux complet** :
  - **PBR (Physically Based Rendering)** : Workflow optimisé via **ORM Packing** (R: Occlusion, G: Roughness, B: Metallic) réduisant les accès mémoire
  - **Unlit** : Matériaux simples pour les objets non éclairés
  - **Toon** : Rendu stylisé avec **Outlines (contours)** et quantification des couleurs réactive aux lumières
- **Éclairage dynamique** : Support de **10 lumières simultanées** (Directionnelles et Ponctuelles avec atténuation physique)
- **Post-Process intégré** : Correction Gamma (2.2) et Tone Mapping (Reinhard)
- **Gestion des shaders** : Compilation automatique GLSL → SPIR-V via `glslc`
- **Textures avancées** : Support des cubemaps, textures 2D, et gestion des mipmaps
- **Système de caméra flexible** : Caméras FPS et orbitale avec contrôle intuitif
- **Éclairage dynamique** : Lumières directionnelles, ponctuelles et spot avec ombres portées

### Système ECS (Entity Component System) puissant

- **EnTT** : Bibliothèque ECS performante et moderne
- **API Fluent** : Création et manipulation d'entités avec une syntaxe intuitive
- **Composants riches** :
  - Transform, Mesh, Model, Camera, Light
  - RigidBody, Colliders (Box, Sphere, Capsule)
  - AudioSource, AudioListener
  - Terrain, ParticleSystem, Skybox/SkySphere
  - NativeScript pour les comportements personnalisés
- **Sérialisation complète** : Sauvegarde et chargement de scènes au format JSON
- **Hiérarchie d'entités** : Système parent-enfant avec propagation des transformations

### Gestion des ressources optimisée

- **ResourceManager** : Chargement et gestion centralisée des assets
- **Chargement asynchrone** : Utilisation du JobSystem pour le chargement en arrière-plan
- **Cache des ressources** : Évite les rechargements inutiles
- **Support de formats multiples** :
  - Modèles 3D : glTF (via fastgltf), OBJ (via tinyobjloader)
  - Textures : JPEG, PNG, KTX2
  - Audio : WAV, MP3, OGG (en développement)

### Systèmes avancés

- **Physique réaliste** : Intégration avec Jolt Physics pour la simulation physique
  - Corps rigides (Static, Dynamic, Kinematic, Character)
  - Collisions complexes avec différents types de colliders
  - Synchronisation automatique entre physique et rendu

- **Audio 3D** : Système audio spatialisé avec support multi-format
  - Sources audio positionnelles
  - Effets de réverbération et occlusion
  - Gestion des listeners multiples

- **Profiling intégré** : Intégration de Tracy pour l'analyse des performances
  - Profiling CPU/GPU détaillé
  - Visualisation en temps réel
  - Optimisation guidée par les données

- **EventBus** : Système de communication découplé entre composants
  - Architecture pub/sub typée
  - Gestion des événements personnalisés
  - Performances optimisées

### Outils de développement

- **Hot Reloading** : Rechargement automatique des shaders et assets
- **ImGui Integration** : Interface utilisateur pour le debugging et l'édition
- **Console de logging** : Système de logging avancé avec spdlog
- **Système de configuration** : Fichier JSON pour la configuration du moteur

### Exemple de code - Création d'une scène PBR

```cpp
// Création d'une scène avec différents matériaux
auto scene = engine->CreateScene();

// Ajout d'une caméra orbitale
auto cameraEntity = scene->createEntity("MainCamera");
auto orbitCam = bb3d::CreateRef<bb3d::OrbitCamera>(45.0f, 1280.0f/720.0f, 0.1f, 100.0f);
cameraEntity.add<bb3d::CameraComponent>(orbitCam);

// Chargement des textures PBR
auto albedoTex = engine->assets().load<bb3d::Texture>("assets/PBR/Bricks092_1K-JPG_Color.jpg");
auto normalTex = engine->assets().load<bb3d::Texture>("assets/PBR/Bricks092_1K-JPG_NormalGL.jpg");
auto roughTex = engine->assets().load<bb3d::Texture>("assets/PBR/Bricks092_1K-JPG_Roughness.jpg");

// Création d'un matériau PBR
auto matPBR = bb3d::CreateRef<bb3d::PBRMaterial>(engine->graphics());
matPBR->setAlbedoMap(albedoTex);
matPBR->setNormalMap(normalTex);
matPBR->setORMMap(ormTex); // Occlusion (R), Roughness (G), Metallic (B)

// Création d'une sphère avec le matériau PBR
auto sphereMesh = bb3d::MeshGenerator::createSphere(engine->graphics(), 1.0f, 64, 64);

// Instanciation de 100 sphères (utilisera l'instancing GPU automatiquement)
for(int i = 0; i < 100; i++) {
    scene->createEntity("Sphere")
        .at({(float)(i % 10), 0, (float)(i / 10)})
        .add<bb3d::MeshComponent>(sphereMesh, matPBR);
}

// Ajout de lumières dynamiques
scene->createDirectionalLight("Sun", {1.0f, 1.0f, 0.9f}, 3.0f);
scene->createPointLight("PointRed", {1.0f, 0.0f, 0.0f}, 100.0f, 20.0f, {5.0f, 2.0f, 0.0f});
```

## 📦 Modélisation 3D & Vertex (Modulaire)

Pour optimiser la bande passante mémoire (Bandwidth) et le Vertex Fetch, le moteur supporte plusieurs layouts de sommets. L'utilisation d'une structure "Uber-Vertex" unique est proscrite pour la production.

### Standard Vertex Layout (SSOT)

Pour garantir la compatibilité entre C++ (`Vertex.hpp`) et GLSL, le layout suivant est **obligatoire** pour tous les shaders standard (PBR, Unlit, Toon) :

| Attribut | Location | Type GLSL | Type C++ | Description |
|----------|----------|-----------|----------|-------------|
| **Position** | `0` | `vec3` | `glm::vec3` | Position du sommet (Model Space) |
| **Normal** | `1` | `vec3` | `glm::vec3` | Normale du sommet |
| **Color** | `2` | `vec3` | `glm::vec3` | Couleur du sommet (Vertex Color) |
| **UV** | `3` | `vec2` | `glm::vec2` | Coordonnées de texture |
| **Tangent** | `4` | `vec4` | `glm::vec4` | Tangente (xyz) + Signe bitangent (w) |

**Attention :** Tout nouveau shader doit respecter strictement cet ordre.

* **Système Flexible :** * Implémenter un mécanisme (Traits ou Templates) pour générer automatiquement les VkVertexInputAttributeDescription et VkVertexInputBindingDescription.  
* **Formats Standards Suggérés :** * **VertexPos :** Uniquement position (pour Shadow Maps, Z-Prepass, Collisions).

## 🔮 Roadmap : Outils & Éditeur

Pour les futures versions, l'outillage sera séparé du Runtime.

* **bb3d::EngineEditor (Hérite de Engine) :** * **Interface :** Utilisation de **ImGui** (avec backend Vulkan/SDL3).  
  * **Fonctionnalités :** * Inspecteur de scène (Arbre des entités).  
    * Éditeur de propriétés (Transform, Material, Physics).  
    * Gizmos de manipulation (Translation/Rotation/Scale) dans la vue 3D.  
  * **Architecture :** L'éditeur s'injecte comme une surcouche de rendu (Overlay) sur le moteur standard.

## 📜 Règles de Codage & Standards

### **1\. Abstraction & Portabilité**

* **API Publique :** Aucun type Vulkan (`vk::...`) ou SDL dans les headers de haut niveau du moteur.  
* **Physique :** Ne pas exposer directement les types du moteur physique sous-jacent (ex: btRigidBody).

### **2\. Gestion de Vulkan (Interne)**

* **Vulkan-Hpp :** Utilisation systématique des wrappers C++ (`vulkan.hpp`).
* **VMA :** Usage exclusif pour l'allocation mémoire.  
* **Synchronisation :** Gestion explicite et documentée.

### **3\. Style C++ (Modern C++ & Modules)**

* **Structure de Fichiers (Approche Hybride) :** * **Interne (Engine) :** Privilégier les **Modules C++** (import/export) pour isoler les composants internes et accélérer la compilation.  
  * **API Publique :** Exposer l'API via des **Headers traditionnels (.hpp)** ou une interface de module propre pour garantir la compatibilité avec le code client (le jeu) quel que soit le build system.  
  * **Règle Absolue :** Une classe majeure par fichier.  
* **Modern C++ Features (C++20/23) :** * **Concepts :** Utiliser les **Concepts** pour contraindre les paramètres de template (template\<typename T\> requires std::integral\<T\>) au lieu de SFINAE.  Utiliser les mots clés c++ (ex: requires, if constexpr, local, constexpr, const). Eviter au maximum l'overhead de fonction (utiliser inline).
  * **Ranges :** Utiliser std::ranges et les vues (std::views) pour la manipulation de collections et les algorithmes (ex: filtrage, transformation) au lieu des boucles brutes.  
  * **Coroutines :** Utiliser les coroutines (co\_await, co\_return) pour les tâches asynchrones (chargement d'assets, scripts de comportement) plutôt que des callbacks complexes.  
* **Standard Library (STL) :** Utilisation intensive et prioritaire de la STL.  
  * **Choix Stratégique des Conteneurs (Performance) :** * **std::vector :** Le choix par défaut absolu. La contiguïté mémoire minimise les "Cache Misses".  
    * **std::array :** Obligatoire si la taille est connue à la compilation (stack allocation, zero-overhead).  
    * **std::unordered\_map :** Préférer à std::map pour les lookups (O(1) moyen vs O(log n)). N'utiliser std::map que si l'ordre des clés est vital.  
    * **std::list :** À éviter totalement sauf cas d'école.  
  * **Concurrency :** Privilégier bb3d::JobSystem. Utiliser std::mutex si nécessaire.  
  * **Algorithmes :** Utiliser \<algorithm\> et \<numeric\> (std::sort, std::transform, etc.) plutôt que des boucles manuelles complexes.  
* **Modern Parameter Passing (Zero-Copy) :** * **Chaînes :** Utiliser std::string\_view au lieu de const std::string&.  
  * **Séquences :** Utiliser std::span\<T\> (C++20) au lieu de const std::vector\<T\>&.  
* **Smart Pointers :** Utiliser `bb3d::Ref` (shared) et `bb3d::Scope` (unique).  
* **Naming :** PascalCase (Classes), camelCase (Méthodes), m\_variable (Privé).  
* **Documentation (Doxygen) :** Tout le code (classes, méthodes, membres publics) doit être documenté systématiquement au format Doxygen (`/** ... */`).
* **Developer Experience (DX) \- Defaults :** * **Règle :** Tous les objets de haut niveau (Components, Resources) doivent être générés avec des **paramètres par défaut fonctionnels**.  
  * **Objectif :** Simplifier la tâche de l'utilisateur. Une instantiation sans argument (ex: entity.add\<Light\>()) doit produire un résultat immédiatement valide et visible (ex: Lumière blanche, intensité 1.0, portée standard) sans nécessiter de configuration complexe obligatoire.


### **4\. Sérialisation & Réflexion (Sauvegarde)**

* **Interface de Sérialisation (Mandatoire) :** * **Exigence :** Toutes les classes définissant l'état du jeu (notamment les **Components**, **Resources** et la **Config**) doivent être sérialisables.  
  * **Implémentation :** Chaque classe doit fournir des méthodes serialize(json& j) et deserialize(const json& j) (ou compatible nlohmann/json) ou s'intégrer dans un système de réflexion statique interne.  
  * **Objectif :** Permettre à Engine::exportScene() de générer un fichier JSON complet représentant l'état exact de la scène (position des entités, paramètres des lumières, chemins des assets) sans perte d'information.

### **5\. Performance (Jeu Vidéo)**

* **Zero-Overhead :** Interdire tout overhead de fonction inutile sur les appels aux APIs de base (Vulkan, SDL3, Jolt). Les wrappers doivent être `inline` ou résolus à la compilation pour garantir une performance identique à l'appel natif.
* **Hot Path Safety :** Pas d'allocations dans update() ou render().  
* **Data-Oriented Design :** Contiguïté mémoire pour les composants (Transform, RigidBody).  
* **Instancing :** Rendu instancié automatique pour les particules et objets répétés.  
* **Compute Shaders :** Utiliser pour le Culling, les Particules et le Skinning si possible.

### **6\. Debugging, Logging & Tests (Outils Internes)**

* **Système de Log & Trace (spdlog) :** * **Bibliothèque :** Utiliser **spdlog**.  
  * **Architecture :** Wrapper l'initialisation dans bb3d::Log. Loggers séparés "CORE" et "CLIENT".  
  * **Macros :** Utiliser BB\_CORE\_INFO(...), BB\_ERROR(...).  
  * **Compile-time Strip :** Configurer SPDLOG\_ACTIVE\_LEVEL pour supprimer les logs en Release.  
* **Profiling Visuel (Tracy) :** * **Outil :** **Tracy Profiler**. C'est le standard pour le profiling Frame/GPU/Memory en C++.  
  * **Macros :** Définir des macros BB\_PROFILE\_FRAME(name) et BB\_PROFILE\_SCOPE(name) qui appellent Tracy.  
  * **Stripping :** Ces macros doivent être définies comme vides (\#define BB\_PROFILE\_SCOPE(name)) si le flag de profiling n'est pas activé (Build Release).  
* **Tests Unitaires (Zéro Dépendance) :** * **Philosophie :** Pas de frameworks externes lourds. Système minimaliste interne (BB\_TEST\_CASE).

## **🔍 Instructions pour l'IA**

1. **Focus Abstraction :** Engine n'inclut jamais `<vulkan/vulkan.h>` ni `<vulkan/vulkan.hpp>`.  
2. **PBR :** Les shaders générés doivent être PBR.  
3. **Maths :** Toujours utiliser GLM.  
4. **Physique :** Interface générique (IPhysicsBackend).  
5. **Animation :** Structures Skinning dans Vertex.  
6. **Optimisation :** std::string\_view, std::span, vector vs list.  
7. **Config & Log :** Implémenter le chargement de engine\_config.json et les macros spdlog/Tracy.  
8. **Architecture :** Intégrer JobSystem et EventBus dans les propositions d'architecture Core.  
9. **Modern C++ :** Utiliser les **Modules** (Interne) et **Headers** (Public), **Concepts**, **Ranges** et **Coroutines** dans le code généré.  
10. **Sérialisation :** Assurer que tout code de composant généré inclut les hooks de sérialisation JSON pour l'export.  
11. **Defaults :** Générer systématiquement des valeurs par défaut valides pour tous les composants.  
12. **Modularité :** N'initialiser les systèmes (Audio, Physique, Jobs) que s'ils sont explicitement activés dans Config.

### **7\. Exemple Complet (Kitchen Sink Demo)**

Voici un fichier main.cpp illustrant l'usage de toutes les fonctionnalités majeures (Core, Audio, Physique, FX, Input) via l'API Fluent.
