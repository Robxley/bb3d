# Design Doc — Refonte Modulaire de l'Éditeur & Thème Dark Pro (Phase 4.1)

- **Date :** 2026-09-26
- **Auteur :** Agent Implémenteur & Engine Tools Architect
- **Branche :** `feat/editor-modular-core`
- **Statut :** PROPOSED

---

## 1. Contexte & Objectif

L'éditeur actuel de `bb3d` réside principalement dans une classe monolithique `ImGuiLayer` (plus de 1100 lignes dans `src/bb3d/core/ImGuiLayer.cpp`), couplée de manière rigide à `Engine.cpp`. Elle mélange :
- Le cycle de vie bas niveau Dear ImGui / Vulkan / SDL3
- La création des polices
- Le dessin en dur de la hiérarchie, de l'inspecteur, du viewport, de la toolbar et des settings
- Des variables d'état globales statiques sujettes aux bugs de concurrence et de cycle de vie (N5, N6, N7 dans `tasks/CODE_REVIEW.md`)

### Objectifs de la Phase 4.1

1. **Découplage architectural :**
   - Créer le bus d'état central `bb3d::editor::EditorContext` encapsulant l'état interactif (scène active, sélection unique & multiple, barycentre pivot, hovered entity, état de simulation).
   - Définir l'interface pure `bb3d::editor::EditorPanel` pour tous les panneaux de l'éditeur.
   - Créer `bb3d::editor::EditorPanelManager` pour gérer le cycle de vie, la mise à jour et le rendu des panneaux enregistrés.
2. **Découpage modulaire :**
   - Extraire les panneaux actuels vers `include/bb3d/editor/panels/` et `src/bb3d/editor/panels/` :
     - `ViewportPanel` : Affichage de la texture RTT du `RenderTarget`, gestion du focus/hover et détection de redimensionnement.
     - `SceneHierarchyPanel` : Arborescence de la scène EnTT, sélection d'entités, création/suppression d'entités.
     - `InspectorPanel` : Inspection et modification des composants d'entités avec isolation des widgets.
     - `ToolbarPanel` : Contrôles de simulation (Play, Pause, Step, Reset).
     - `SceneSettingsPanel` : Paramètres d'environnement (brouillard, skybox, paramètres caméra).
     - `ConsolePanel` : Visualisation temps réel des logs spdlog avec filtrage.
3. **Design System & Ergonomie Dark Pro :**
   - Créer `bb3d::editor::EditorStyle` appliquant les tokens d'élévation, les couleurs sombres professionnelles (fond `#1A1D20`, surfaces `#22252A`, accents bleu acier `#2D6CDF`), les arrondis et espacements spécifiés dans `docs/editor/EDITOR_UI_UX_DESIGN_SYSTEM.md`.
4. **Éradication des bugs de revue statique :**
   - **N5** : Suppression du bloc dupliqué vide `if (entity.has<LightComponent>())`.
   - **N6** : Élimination de la variable statique mutable `partCol` dans l'inspecteur de particules.
   - **N7** : Élimination de `s_loadConfig` statique non thread-safe.

---

## 2. Architecture & Composants

```
                                +-------------------+
                                |    ImGuiLayer     |  (Infrastructure Vulkan/SDL3/Dockspace)
                                +---------+---------+
                                          |
                        +-----------------+-----------------+
                        |                                   |
              +---------v---------+               +---------v---------+
              |    EditorStyle    |               | EditorPanelManager|
              | (Dark Pro Theme)  |               +---------+---------+
              +-------------------+                         |
                                                            | gère
                                         +------------------+------------------+
                                         |                  |                  |
                                 +-------v-------+  +-------v-------+  +-------v-------+
                                 | ViewportPanel |  |HierarchyPanel |  |InspectorPanel | ...
                                 +-------+-------+  +-------+-------+  +-------+-------+
                                         |                  |                  |
                                         +------------------+------------------+
                                                            | observe & modifie
                                                  +---------v---------+
                                                  |   EditorContext   |
                                                  +-------------------+
                                                  | - Scene*          |
                                                  | - Selection       |
                                                  | - SimulationState |
                                                  | - Engine*         |
                                                  +-------------------+
```

### 2.1. `EditorContext`
```cpp
namespace bb3d::editor {

enum class SimulationState { Stopped, Playing, Paused };

class EditorContext {
public:
    void setActiveScene(Scene* scene);
    [[nodiscard]] Scene* getActiveScene() const noexcept;

    // Sélection
    void selectEntity(Entity entity);
    void deselectEntity(Entity entity);
    void clearSelection();
    [[nodiscard]] Entity getSelectedEntity() const;
    [[nodiscard]] const std::vector<Entity>& getSelectedEntities() const noexcept;
    [[nodiscard]] bool isSelected(Entity entity) const;

    // Hover
    void setHoveredEntity(Entity entity) noexcept;
    [[nodiscard]] Entity getHoveredEntity() const noexcept;

    // Pivot & Barycentre
    [[nodiscard]] glm::vec3 getSelectionCenter() const;

    // Simulation
    void setSimulationState(SimulationState state);
    [[nodiscard]] SimulationState getSimulationState() const noexcept;

    // Liens Moteur
    void setEngine(class Engine* engine) noexcept;
    [[nodiscard]] class Engine* getEngine() const noexcept;
};

} // namespace bb3d::editor
```

### 2.2. Interface `EditorPanel`
```cpp
namespace bb3d::editor {

class EditorPanel {
public:
    virtual ~EditorPanel() = default;

    virtual void onAttach(EditorContext& context) { m_context = &context; }
    virtual void onDetach() { m_context = nullptr; }
    virtual void onUpdate([[maybe_unused]] float dt) {}
    virtual void onImGuiRender() = 0;
    virtual void onEvent([[maybe_unused]] const SDL_Event& event) {}

    [[nodiscard]] virtual std::string_view getId() const = 0;
    [[nodiscard]] virtual std::string_view getTitle() const = 0;
    [[nodiscard]] virtual const char* getIcon() const { return ""; }

    [[nodiscard]] bool& isOpen() noexcept { return m_isOpen; }
    [[nodiscard]] bool isOpen() const noexcept { return m_isOpen; }
    void setOpen(bool open) noexcept { m_isOpen = open; }

protected:
    EditorContext* m_context = nullptr;
    bool m_isOpen = true;
};

} // namespace bb3d::editor
```

### 2.3. `EditorPanelManager`
```cpp
namespace bb3d::editor {

class EditorPanelManager {
public:
    explicit EditorPanelManager(EditorContext& context);
    ~EditorPanelManager();

    template <typename T, typename... Args>
    T* addPanel(Args&&... args);

    void removePanel(std::string_view id);
    [[nodiscard]] EditorPanel* getPanel(std::string_view id) const;

    template <typename T>
    [[nodiscard]] T* getPanel() const;

    void onUpdate(float dt);
    void onImGuiRender();
    void onEvent(const SDL_Event& event);

    [[nodiscard]] const std::vector<Scope<EditorPanel>>& getPanels() const noexcept;

private:
    EditorContext& m_context;
    std::vector<Scope<EditorPanel>> m_panels;
};

} // namespace bb3d::editor
```

---

## 3. Performance Moteur 3D (Règle Suprême 0)

1. **Zéro-Allocation dans la boucle de rendu :**
   - Les panneaux sont alloués une seule fois au démarrage dans `EditorPanelManager`.
   - `onImGuiRender()` et `onUpdate()` n'allouent aucune mémoire dynamique.
   - Les vecteurs de sélection d'`EditorContext` réservent leur capacité à l'avance (`reserve(32)`).
2. **Localité de cache :**
   - Stockage contigu des pointeurs de panneaux dans un `std::vector<Scope<EditorPanel>>`.
   - Parcours séquentiel des panneaux sans recherche de chaîne sur le chemin critique.
3. **Opacité de l'API :**
   - Les types Vulkan restent strictement confinés au backend `ImGuiLayer` (chargement de textures pour ImGui via `addTexture`). Les panneaux d'inspection n'incluent pas d'en-têtes Vulkan.

---

## 4. Plan de Test TDD

Un test unitaire dédié `tests/unit_test_38_editor_context.cpp` validera :
1. Cycle de vie de `EditorContext` : affectation de scène, simulation state.
2. Système de sélection : sélection unique, multi-sélection, déselection, vérification d'unicité, calcul de barycentre 3D pour la multi-sélection.
3. `EditorPanelManager` : enregistrement de panneaux polymorphiques, cycle `onAttach`/`onDetach`, dispatch de `onUpdate` et `onEvent`.
4. Intégration globale sans fuite de mémoire ni variables statiques partagées.
