# 🤖 Directives & Standards Agents - Projet biobazard3d (`bb3d`)

Ce document définit les standards techniques, l'architecture, les compétences et les contraintes obligatoires pour **tout agent IA** intervenant sur le moteur **biobazard3d**.

---

## 🛠️ Stack Technique

* **Nom du Projet :** biobazard3d | **Namespace :** `bb3d`
* **Langage :** C++20 (ou supérieur) - Utilisation intensive des smart pointers (`bb3d::Ref<T>` / `bb3d::Scope<T>`) et de la RAII.
* **Windowing / Input :** SDL3.
* **Graphics API :** Vulkan 1.3+ / 1.4 via **Vulkan-Hpp** (Dynamic Rendering, Synchronization2, VMA).
* **Gestion Mémoire :** Vulkan Memory Allocator (VMA).
* **Maths :** GLM (`glm::vec3`, `glm::quat`, `glm::mat4`).
* **Physique :** Jolt Physics.
* **Audio :** miniaudio (Header-only) ou OpenAL Soft.
* **Format Scène/Assets :** glTF 2.0 (`.gltf`, `.glb`), Wavefront (`.obj`).
* **Configuration / Sauvegarde :** JSON (`nlohmann/json`).
* **Logging / Profiling :** spdlog, Tracy Profiler (`BB_PROFILE_SCOPE`).
* **Build System :** CMake (3.20+), CTest. Sortie des binaires dans `bin/`.

---

## 🧭 Organisation des Dossiers & Règle d'Étanchéité

* **`docs/` (100% Documentation Technique) :** Réservé exclusivement aux guides d'architecture pérennes, spécifications et rapports d'audit (ex: `docs/vulkan_audit/RAPPORT_AUDIT_VULKAN_MODERNE.md`). **Aucun plan ni tâche ne doit être stocké dans `docs/`.**
* **`tasks/` (100% Suivi & Pilotage des Développements) :**
  * `ROADMAP.md` : Backlog général et vision stratégique.
  * `CODE_REVIEW.md` : Catalogue des bugs statiques répertoriés (B1 à B28).
  * `HISTORY.md` : Grand livre chronologique compact (3 lignes max par tâche terminée).
  * `templates/` : Gabarit officiel de tâche et grille de revue croisée (`TASK_REVIEW_TEMPLATE.md`).
  * `active/` : Fiches de tâches en cours de traitement.
  * `archive/` : Historique des tâches complétées et archivées.

---

## 📦 Annuaire des Compétences Déclarées (`.agents/skills/`)

L'agent **DOIT** consulter et activer les compétences locales selon le besoin :

| Compétence | Chemin | Quand l'utiliser ? |
| :--- | :--- | :--- |
| **`brainstorming`** | `.agents/skills/brainstorming/` | **Obligatoire** avant tout travail créatif, ajout de fonctionnalité ou changement d'architecture. Établit le Design Doc dans `tasks/active/`. |
| **`planification-revue`** | `.agents/skills/planification-revue/` | **Obligatoire** dès qu'un design est validé. Découpe en TDD et fournit la grille de revue croisée (Implémenteur vs Reviewers). |
| **`cpp-pro`** | `.agents/skills/cpp-pro/` | Pour tout code C++20/23 : Zero-Copy (`std::span`, `std::string_view`), initialisation désignée, zéro-allocation hot-path, conteneurs cache-friendly. |
| **`vulkan-cpp`** | `.agents/skills/vulkan-cpp/` | Pour toute manipulation Vulkan : `StructureChain`, Dynamic Rendering, `pipelineBarrier2` (Sync2), Timeline Semaphores, Push Descriptors, multi-streams sommets. |
| **`createur-de-competences`** | `.agents/skills/createur-de-competences/` | Pour concevoir et générer de nouvelles compétences au standard officiel. |

---

## 🔄 Workflow Obligatoire pour Toute Intervention

Pour toute nouvelle fonctionnalité, refactoring ou correction de bug, l'IA **DOIT OBLIGATOIREMENT** suivre ce cycle :

```
1. Brainstorming    ➔ Discuter de l'intention, explorer 2 à 3 approches, rédiger tasks/active/YYYY-MM-DD-<sujet>-design.md
2. Planification    ➔ Invoquer planification-revue, dupliquer TASK_REVIEW_TEMPLATE.md dans tasks/active/
3. Approbation      ➔ Obtenir la validation formelle de l'architecture par l'utilisateur
4. Exécution TDD    ➔ Rédiger le test unitaire d'abord, coder le minimum, valider CTest, commiter de manière atomique
5. Revue Croisée    ➔ Valider les checkpoints Implémenteur et Reviewer dans la fiche
6. Clôture          ➔ Consigner 1 entrée compacte (3 lignes) dans tasks/HISTORY.md et déplacer vers tasks/archive/
```

---

## 📜 Règles de Codage & Standards Critiques

1. **Opacité de l'API Publique :** Le code client (`Engine`, `Scene`, `Component`, `Mesh`) ne doit **JAMAIS** inclure de headers Vulkan (`<vulkan/...>`) ni manipuler des types `vk::*` ou `SDL_*`.
2. **Zero-Allocation dans le Hot Path :** Aucune allocation dynamique dans `update()` ou `render()`. Pré-allouer via `reserve()` ou buffers réutilisables.
3. **Multi-Streams Sommets :** L'utilisation d'une structure Uber-Vertex unique est **proscrite en production**. Séparer `VertexPos` (12 octets) pour les shadow maps, le z-prepass et le picking.
4. **Synchronisation Vulkan Moderne :** Bannir `pipelineBarrier` (Vulkan 1.0). Utiliser exclusivement `pipelineBarrier2` avec `vk::DependencyInfo` et `vk::ImageMemoryBarrier2`.
5. **Zero Attente Bloquante CPU :** Bannir `waitIdle()` et `waitForFences()` synchrones pour les transferts de textures et maillages.
6. **Anglais Technique :** Tout le code source, noms de variables, commentaires, messages de log (`spdlog`) et documentation Doxygen doivent être rédigés en **anglais**.
7. **Sécurité vs Performance :** Envelopper systématiquement les vérifications coûteuses, traces lourdes et asserts dans des blocs `#if defined(BB3D_DEBUG)`.

---

## 🛠️ Commandes Utiles

```bash
# Configuration & Build
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug -j

# Exécution des tests automatisés
ctest --test-dir build -C Debug --output-on-failure
```
