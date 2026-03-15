# biobazard3d 🚀

**biobazard3d** est un moteur de jeu 3D moderne écrit en C++20, utilisant Vulkan comme backend graphique. Le projet suit une approche de développement séquentiel avec validation par tests unitaires à chaque étape.

## 📖 À propos

biobazard3d est conçu pour être un moteur de jeu complet avec les caractéristiques suivantes :

- **Architecture moderne** : C++20, Vulkan 1.3, ECS (Entity Component System).
- **Rendu avancé** : PBR (Physically Based Rendering), gestion des matériaux, ombres dynamiques (Cascaded Shadow Maps avec PCF).
- **Système de physique** : Intégration performante avec Jolt Physics.
- **Gestion des ressources** : Chargement asynchrone, cache des assets, empreinte RAM optimisée.
- **Système de scène** : Sérialisation/deserialization JSON, hiérarchie d'entités.
- **Outils de développement** : Profiling avec Tracy, logging avec spdlog hautement configurable.

## 🔧 Technologies et dépendances

Le projet utilise les technologies et bibliothèques suivantes, toutes gérées automatiquement via CMake FetchContent :

### Dépendances principales

| Technologie | Version | Description | Statut |
|-------------|---------|-------------|--------|
| **C++** | 20 | Standard C++20 avec compilation stricte | ✅ Requis |
| **Vulkan** | 1.3+ | Backend graphique moderne | ✅ Requis |
| **SDL3** | 3.4.0 | Gestion des fenêtres et des entrées | ✅ Intégré |
| **spdlog** | 1.17.0 | Système de logging performant (Stripping compile-time) | ✅ Intégré |
| **nlohmann_json** | 3.12.0 | Parsing et génération JSON | ✅ Intégré |
| **Tracy** | 0.13.1 | Profiling et optimisation | ✅ Intégré |
| **VulkanMemoryAllocator** | 3.3.0 | Gestion de la mémoire Vulkan | ✅ Intégré |
| **GLM** | 1.0.3 | Bibliothèque mathématique pour l'infographie | ✅ Intégré |
| **stb** | master | Chargement d'images | ✅ Intégré |
| **fastgltf** | 0.8.0 | Chargement de modèles 3D au format glTF | ✅ Intégré |
| **tinyobjloader** | release | Chargement de modèles 3D au format OBJ | ✅ Intégré |
| **EnTT** | 3.13.2 | Bibliothèque ECS (Entity Component System) | ✅ Intégré |
| **Jolt Physics** | master | Moteur de physique 3D haute performance | ✅ Intégré |

### Dépendances optionnelles

| Technologie | Version | Description | Statut |
|-------------|---------|-------------|--------|
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

- **Vulkan SDK** : Doit être installé manuellement et disponible dans le PATH.
- **CMake 3.20+** : Requis pour le système de build.
- **Compilateur C++20** : MSVC 2022+, GCC 11+, ou Clang 14+.

## 🛠️ Configuration requise

- **Système d'exploitation** : Windows (testé), Linux (supporté).
- **Architecture CPU** : X64 (AVX2 recommandé pour Jolt).
- **Vulkan SDK** : Version 1.3 ou supérieure.

## 🚀 Installation et compilation

### 1. Configurer l'environnement

Assurez-vous que `VULKAN_SDK` est défini et que `glslc` est dans votre PATH.

### 2. Configurer et compiler (Build Rapide)

Le projet utilise les **Unity Builds** pour une compilation ultra-rapide.

```powershell
# Configuration
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug

# Compilation (8 threads)
cmake --build build --config Debug --parallel 8
```

### 3. Options de compilation avancées

```powershell
# Build Release optimisé (Supprime les logs TRACE/DEBUG)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
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

### Moteur de rendu Vulkan Haute Performance

- **Unity Build** : Compilation accélérée par regroupement de fichiers source.
- **JobSystem & Multithreading** : Pool de threads pour le chargement d'assets, le culling, et la création de maillages procéduraux.
- **GPU Instancing (SSBO)** : Rendu de milliers d'objets en un seul Draw Call.
- **Batching Automatique** : Optimisation de l'état du pipeline pour réduire les changements de contexte GPU.
- **RAM Footprint Optimization** : Les données CPU des Meshs sont libérées après l'upload VRAM pour économiser la mémoire système.
- **Frustum & Horizon Culling** : Élimination précise des objets hors champ (Frustum) et des faces arrières cachées typiques des planètes (Horizon), optimisé sur le CPU.
- **Post-Process** : Rendu Offscreen avec Render Scale ajustable et Tone Mapping.
- **Système de matériaux complet** :
  - **PBR (Physically Based Rendering)** : Workflow optimisé via **ORM Packing**.
  - **Unlit** : Matériaux simples.
  - **Toon** : Rendu stylisé.
- **Éclairage dynamique** : Support de lumières directionnelles (CSM Shadows prévu) et Ponctuelles.
- **Génération Procédurale** : Support de géométries massives complexes type Cube-Sphères multi-biomes (Planètes).

### Physique & Simulation (Jolt)

- **Intégration complète** : Corps rigides (Static, Dynamic, Kinematic).
- **Colliders variés** : Box, Sphere, Capsule et MeshCollider (Convexe ou précis).
- **Performance** : Utilisation intensive du multi-threading pour la simulation.

### Logging & Debugging

- **Macro BB3D_DEBUG** : Vérifications de sécurité actives uniquement en Debug (zero-overhead en Release).
- **Stripping Automatique** : Les logs de bas niveau (`TRACE`, `DEBUG`) sont supprimés du binaire en mode Release pour une performance maximale.
- **Verbosité dynamique** : Configurable via `engine_config.json`.

### Système ECS (Entity Component System) puissant

- **EnTT** : Bibliothèque ECS performante et moderne
- **API Fluent** : Création et manipulation d'entités avec une syntaxe intuitive
- **Composants riches** : Transform, Mesh, Model, Camera, Light, RigidBody, AudioSource, etc.
- **Sérialisation complète** : Sauvegarde et chargement de scènes au format JSON

### 🧪 Tests et Démonstrations

| Test | Description | Commande |
|------|-------------|----------|
| `unit_test_04_hello_triangle` | Premier triangle Vulkan | `./tests/Debug/unit_test_04_hello_triangle` |
| `unit_test_06_rotating_cube` | Cube 3D avec éclairage | `./tests/Debug/unit_test_06_rotating_cube` |
| `unit_test_10_obj_loader` | Chargement de modèles OBJ | `./tests/Debug/unit_test_10_obj_loader` |
| `unit_test_11_gltf_loader` | Modèles glTF complexes | `./tests/Debug/unit_test_11_gltf_loader` |
| `unit_test_14_interactive_cameras` | Caméras interactives | `./tests/Debug/unit_test_14_interactive_cameras` |
| `unit_test_16_materials` | Matériaux PBR/Unlit/Toon | `./tests/Debug/unit_test_16_materials` |
| `demo_engine` | **DÉMO COMPLÈTE** (Physique, Skybox, Caméras) | `./tests/Debug/demo_engine` |

### Exemple de code - Création d'une scène

```cpp
// Création d'une scène
auto scene = engine->CreateScene();

// Ajout d'une caméra orbitale
auto cameraEntity = scene->createOrbitCamera("MainCamera", 45.0f, 16.0f/9.0f, {0, 5, 10}, 20.0f);

// Chargement d'un modèle glTF et optimisation RAM
auto model = engine->assets().load<Model>("assets/models/ant.glb");
model->normalize();
model->releaseCPUData(); // Libère la RAM après upload GPU

// Création d'une entité physique
auto entity = scene->createEntity("Ant");
entity.add<ModelComponent>(model);
entity.add<RigidBodyComponent>().type = BodyType::Dynamic;
entity.add<BoxColliderComponent>(); // Collider automatique
engine->physics().createRigidBody(entity);

// Ajout de lumières
scene->createDirectionalLight("Sun", {1.0f, 1.0f, 0.9f}, 3.0f);
```

## 📜 Règles de Codage & Standards

### Macro BB3D_DEBUG
Toute vérification de sécurité coûteuse (ex: validation d'état complexe) doit être enveloppée :
```cpp
#if defined(BB3D_DEBUG)
    // Vérification uniquement en Debug
    if (mesh->isCPUDataReleased()) BB_CORE_ERROR("...");
#endif
```

### Gestion de la RAM Mesh
Pour les objets statiques, libérez la RAM après initialisation :
```cpp
auto model = assets.load<Model>("...");
model->normalize();
model->releaseCPUData(); // Libère les vertices/indices du côté CPU
```