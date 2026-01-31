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

Le projet utilise les technologies et bibliothèques suivantes :

| Technologie | Version | Description |
|-------------|---------|-------------|
| **C++** | 20 | Standard C++20 avec compilation stricte |
| **Vulkan** | 1.3 | Backend graphique moderne |
| **SDL3** | 3.4.0 | Gestion des fenêtres et des entrées |
| **spdlog** | 1.17.0 | Système de logging performant |
| **nlohmann_json** | 3.12.0 | Parsing et génération JSON |
| **Tracy** | 0.13.1 | Profiling et optimisation |
| **VulkanMemoryAllocator** | 3.3.0 | Gestion de la mémoire Vulkan |
| **GLM** | 1.0.3 | Bibliothèque mathématique pour l'infographie |
| **stb** | master | Chargement d'images |
| **fastgltf** | 0.8.0 | Chargement de modèles 3D au format glTF |
| **tinyobjloader** | release | Chargement de modèles 3D au format OBJ |
| **EnTT** | 3.13.2 | Bibliothèque ECS (Entity Component System) |

## 🛠️ Configuration requise

- **Système d'exploitation** : Windows (testé), Linux (théoriquement supporté)
- **Compilateur** : MSVC (Windows) ou GCC/Clang (Linux) avec support C++20
- **Vulkan SDK** : Version 1.3 ou supérieure
- **CMake** : Version 3.20 ou supérieure
- **Git** : Pour la gestion des dépendances

## 🚀 Installation et compilation

### 1. Cloner le dépôt

```bash
git clone https://github.com/votre-utilisateur/biobazard3d.git
cd biobazard3d
```

### 2. Installer les dépendances

Le projet utilise CMake avec FetchContent pour gérer automatiquement les dépendances. Assurez-vous d'avoir :

- Vulkan SDK installé et disponible dans le PATH
- Un compilateur C++20 compatible
- CMake 3.20 ou supérieur

### 3. Configurer et compiler

#### Sur Windows (avec Visual Studio)

```bash
mkdir build
cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Debug
```

#### Sur Linux

```bash
mkdir build
cd build
cmake -G "Unix Makefiles" ..
cmake --build . --config Debug
```

### 4. Exécuter les tests

Le projet inclut une série de tests unitaires qui valident chaque composant :

```bash
ctest -C Debug -V
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

### Moteur de rendu Vulkan

- **Pipeline graphique moderne** : Utilisation de Vulkan 1.3 avec Dynamic Rendering
- **Gestion des shaders** : Compilation automatique des shaders GLSL en SPIR-V
- **Textures et matériaux** : Support des textures, PBR, et différents types de matériaux
- **Système de caméra** : Caméras FPS et orbitale avec gestion des entrées

### Système ECS (Entity Component System)

- **EnTT** : Bibliothèque ECS performante et moderne
- **API Fluent** : Création et manipulation d'entités avec une syntaxe intuitive
- **Sérialisation** : Sauvegarde et chargement de scènes au format JSON

### Gestion des ressources

- **ResourceManager** : Chargement et gestion centralisée des assets
- **Chargement asynchrone** : Utilisation du JobSystem pour le chargement en arrière-plan
- **Cache des ressources** : Évite les rechargements inutiles

### Systèmes avancés

- **Physique** : Intégration avec Jolt Physics pour la simulation physique
- **Audio** : Système audio avec support 3D (en développement)
- **Profiling** : Intégration de Tracy pour l'analyse des performances
- **EventBus** : Système de communication entre composants

## 📚 Documentation

La documentation complète est disponible dans le code source sous forme de commentaires Doxygen. Une documentation plus détaillée est en cours de développement.

## 🤝 Contribution

Les contributions sont les bienvenues ! Veuillez suivre ces étapes :

1. Fork le projet
2. Créez une branche pour votre fonctionnalité (`git checkout -b feature/AmazingFeature`)
3. Commitez vos changements (`git commit -m 'Add some AmazingFeature'`)
4. Poussez vers la branche (`git push origin feature/AmazingFeature`)
5. Ouvrez une Pull Request

## 📜 Licence

Ce projet est sous licence MIT. Voir le fichier `LICENSE` pour plus de détails.

## 🎯 Roadmap

Consultez le fichier [roadmap.md](roadmap.md) pour voir les prochaines étapes de développement et les fonctionnalités prévues.

## 📞 Contact

Pour toute question ou suggestion, n'hésitez pas à ouvrir une issue sur GitHub ou à contacter les mainteneurs du projet.

---

*Made with ❤️ and C++20* 🚀