---
name: cpp-pro
description: "Rédige du code C++ idiomatique haute performance avec les fonctionnalités modernes (C++20/C++23), RAII strict, zéro-allocation dans le hot path, concepts, std::span/std::string_view et Data-Oriented Design."
---

# Expert C++ Haute Performance (cpp-pro)

## Quand utiliser cette compétence
- Pour tout développement, refactoring, optimisation ou revue de code C++20/C++23 dans le moteur `biobazard3d`.
- Pour concevoir des architectures zero-overhead, data-oriented et sans allocation dans les chemins critiques.

## Directives Fondamentales

### 1. Modern C++20/C++23 & Zero-Copy
- **Passage de paramètres (Zero-Copy) :** 
  - Utiliser `std::string_view` pour les chaînes de caractères en lecture seule (bannir `const std::string&`).
  - Utiliser `std::span<const T>` pour les tableaux contigus en lecture seule (bannir `const std::vector<T>&` en entrée de fonction).
- **Initialisation désignée (C++20) :** Préférer les *designated initializers* (`Type{.param = val}`) pour les structures de données, descripteurs et configurations.
- **Concepts & Templates :** Contraindre systématiquement les paramètres de template avec des `requires` / `concepts` plutôt que SFINAE.
- **Attributs :** Déclarer `[[nodiscard]]` systématiquement sur les accesseurs et fonctions retournant un état critique ou une ressource allouée.
- **Compile-Time First :** Maximiser l'usage de `constexpr` / `consteval`, `if constexpr` pour résoudre les calculs à la compilation.

### 2. Performance Moteur & Hot Path Safety
- **Zero-Allocation dans le Hot Path :** Aucune allocation dynamique (`new`, `std::vector::push_back` provoquant une réallocation, `std::string`, `make_shared`) dans les boucles `update()` et `render()`. Pré-allouer via `reserve()` ou pools réutilisables.
- **Choix des conteneurs STL (Cache Friendly) :**
  - `std::vector` : Le choix par défaut absolu (contiguïté mémoire, respect du cache L1/L2).
  - `std::array` : Obligatoire si la taille est connue à la compilation (stack allocation, zéro overhead).
  - `std::unordered_map` : Pour les lookups O(1). Utiliser `std::map` uniquement si l'ordre des clés est vital.
  - `std::list` : **Proscrit** en production (dissémination mémoire, cache misses majeurs).
- **RAII & Smart Pointers :** Utiliser les alias du moteur (`bb3d::Ref<T>` pour `std::shared_ptr`, `bb3d::Scope<T>` pour `std::unique_ptr`). Privilégier la Règle de Zéro (agrégats à destruction automatique).

### 3. Standards de Documentation & Débogage
- **Anglais obligatoire :** Tout le code, noms de variables, commentaires, documentation Doxygen (`/** ... */`) et messages de log (`spdlog`) doivent être rédigés en **anglais**.
- **Sécurité Release vs Debug :** Envelopper les vérifications coûteuses, traces lourdes et assertions dans des blocs `#if defined(BB3D_DEBUG)` pour garantir un coût strictement nul en Release.
- **Logs temporaires :** Tout fichier temporaire de log/trace doit être écrit dans le dossier `bin/` ou `unit_test_logs/` pour ne pas polluer la racine du dépôt.
- **Profilage :** Instrumenter les fonctions clés avec les macros Tracy (`BB_PROFILE_SCOPE("Name")`).

**Règle d'or :** Suis scrupuleusement les *C++ Core Guidelines*. Préfère systématiquement les erreurs à la compilation (compile-time) plutôt que les erreurs à l'exécution (runtime).
