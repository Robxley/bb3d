# Plan d'implémentation de Component Registry (Sérialisation Extensible)

**Objectif :** Rendre la sérialisation et désérialisation du moteur 3D (`SceneSerializer`) entièrement extensibles pour sauvegarder/charger l'état complet du jeu *Astro Bazard* sans altérer le moteur 'core'.

**Architecture :** Création d'un patron de conception **Registry** statique dans le moteur (`bb3d::ComponentRegistry`). Ce registre stocke des pointeurs de fonctions ("type-erased" lambdas) pour la sérialisation et la désérialisation. Le jeu appelle `bb3d::ComponentRegistry::Register<T>("Name")` au démarrage. Le `SceneSerializer` itère ensuite sur ce registre pour sauvegarder et charger dynamiquement ces composants personnels.

**Stack Technique :** C++20, templates, std::unordered_map, pointeurs de fonctions, nlohmann::json.

---

### Tâche 1: Créer le `ComponentRegistry` dans le moteur

**Fichiers concernés :**
- Créer : `c:/dev/bb3d/include/bb3d/scene/ComponentRegistry.hpp`
- Modifier : `c:/dev/bb3d/apps/bb3d_hello_world/main.cpp` (utiliser comme test sandbox)

**Étape 1 : Écrire le test qui échoue**
*Modification temporaire du main.cpp de bb3d_hello_world pour tester l'interface de registre.*
```cpp
// Dans c:/dev/bb3d/apps/bb3d_hello_world/main.cpp
#include "bb3d/scene/ComponentRegistry.hpp"
#include "bb3d/scene/Entity.hpp"
#include <nlohmann/json.hpp>

struct TestComponent {
    int data = 42;
    void serialize(nlohmann::json& j) const { j["data"] = data; }
    void deserialize(const nlohmann::json& j) { if (j.contains("data")) data = j["data"]; }
};

// Dans le main:
bb3d::ComponentRegistry::Register<TestComponent>("TestComponent");
nlohmann::json j;
// Expected compile error car ComponentRegistry n'existe pas encore.
```

**Étape 2 : Lancer le test pour vérifier l'échec**
Commande : `cmake --build build --config Debug --target bb3d_hello_world`
Attendu : Échec à la compilation (include introuvable).

**Étape 3 : Implémentation Minimale**
```cpp
// c:/dev/bb3d/include/bb3d/scene/ComponentRegistry.hpp
#pragma once

#include "bb3d/scene/Entity.hpp"
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <string>

namespace bb3d {

    using SerializeCallback = void(*)(Entity, nlohmann::json&);
    using DeserializeCallback = void(*)(Entity, const nlohmann::json&);

    struct CustomComponentHandlers {
        SerializeCallback serialize;
        DeserializeCallback deserialize;
    };

    class ComponentRegistry {
    public:
        template<typename T>
        static void Register(const std::string& name) {
            s_handlers[name] = {
                [](Entity entity, nlohmann::json& j) {
                    if (entity.has<T>()) {
                        nlohmann::json c;
                        entity.get<T>().serialize(c);
                        j[name] = c;
                    }
                },
                [](Entity entity, const nlohmann::json& j) {
                    if (j.contains(name)) {
                        entity.add<T>().deserialize(j[name]);
                    }
                }
            };
        }

        static const std::unordered_map<std::string, CustomComponentHandlers>& GetHandlers() {
            return s_handlers;
        }

    private:
        inline static std::unordered_map<std::string, CustomComponentHandlers> s_handlers;
    };
}
```

**Étape 4 : Lancer le test pour vérifier le succès**
Commande : `cmake --build build --config Debug --target bb3d_hello_world`
Attendu : Compilation réussie (PASS).

**Étape 5 : Commit**
```bash
git add c:/dev/bb3d/include/bb3d/scene/ComponentRegistry.hpp
git commit -m "feat(core): ajout du ComponentRegistry pour la sérialisation dynamique"
```

---

### Tâche 2: Intégrer le Registre au `SceneSerializer`

**Fichiers concernés :**
- Modifier : `c:/dev/bb3d/src/bb3d/scene/SceneSerializer.cpp`

**Étape 1 : Écrire le test qui échoue**
Inutile ici, car on modifie directement le `SceneSerializer` dont l'output est vérifié visuellement ou en test d'intégration. Nous provoquons un changement qui sera testé par la compilation et l'exécution de l'éditeur plus tard. L'étape de test consistera à s'assurer de sa bonne compilation.

**Étape 2 : Lancer la compilation (Vérification préalable)**
Commande : `cmake --build build --config Debug --target biobazard3d`
Attendu : Succès de la précédente étape.

**Étape 3 : Implémentation Minimale**
*Modification de `SceneSerializer.cpp` :*
```cpp
// 1. Ajouter l'include tout en haut :
#include "bb3d/scene/ComponentRegistry.hpp"

// 2. Dans SerializeEntity(), ajouter à la fin :
for (const auto& [compName, handler] : ComponentRegistry::GetHandlers()) {
    handler.serialize(entity, j);
}

// 3. Dans SceneSerializer::deserialize(), à l'intérieur de la boucle des entités (après les composants hardcodés) :
for (const auto& [compName, handler] : ComponentRegistry::GetHandlers()) {
    handler.deserialize(entity, e);
}
```

**Étape 4 : Lancer le test pour vérifier le succès**
Commande : `cmake --build build --config Debug --target biobazard3d`
Attendu : Compilation réussie (PASS).

**Étape 5 : Commit**
```bash
git add c:/dev/bb3d/src/bb3d/scene/SceneSerializer.cpp
git commit -m "feat(core): intégration du ComponentRegistry dans le SceneSerializer"
```

---

### Tâche 3: Vérification dans Astro Bazard (Gameplay)

**Fichiers concernés :**
- Modifier : `c:/dev/bb3d/apps/astro_bazard/main.cpp`

**Étape 1 : Enregistrement des Composants**
*Ajouter dans le main.cpp de astro_bazard juste avant Engine::Create() ou juste après l'initialisation :*
```cpp
#include "bb3d/scene/ComponentRegistry.hpp"
// Dans le main(), avant la création de la boucle ou au démarrage
bb3d::ComponentRegistry::Register<astrobazard::SpaceshipControllerComponent>("SpaceshipController");
bb3d::ComponentRegistry::Register<astrobazard::CommRelayComponent>("CommRelay");
```

**Étape 2 : Lancer la compilation pour vérifier le succès**
Commande : `cmake --build build --config Debug --target astro_bazard`
Attendu : Compilation réussie (PASS).

**Étape 3 : Commit**
```bash
git add c:/dev/bb3d/apps/astro_bazard/main.cpp
git commit -m "feat(game): enregistrement des composants Astro Bazard via ComponentRegistry"
```
