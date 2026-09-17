# Plan d'implémentation de [Réseau COMM-LINK : Caméra Multitâche]

**Objectif :** Implémenter le système de relais de communication permettant au joueur de basculer instantanément la caméra entre son vaisseau spatial et les différentes planètes colonisées.

**Architecture :** Création d'un composant `CommRelayComponent` pour marquer les entités "ciblables". La classe `SmartCamera` sera modifiée pour accepter une cible dynamique (au lieu d'être fixée au vaisseau) et gérer des transitions. Le script principal dans `main.cpp` s'occupera de cycler entre les cibles lorsque le joueur appuie sur une touche dédiée (ex: `C`).

**Stack Technique :** C++20, ECS (EnTT/Custom), bb3d::SmartCamera, bb3d::EventBus (optionnel pour les inputs).

---

### Tâche 1: Création du composant `CommRelayComponent`

**Fichiers concernés :**
- Créer : `apps/astro_bazard/CommRelay.hpp`
- Modifier : `apps/astro_bazard/main.cpp` (pour l'inclusion)

**Étape 1 : Créer le header du composant**
```cpp
// apps/astro_bazard/CommRelay.hpp
#pragma once
#include <string>

namespace astrobazard {

struct CommRelayComponent {
    std::string name = "Relais Inconnu";
    bool isActive = true;
};

} // namespace astrobazard
```

**Étape 2 : Mettre à jour CMakeLists (si nécessaire) et vérifier la compilation**
Commande : `cmake --build build --target astro_bazard`
Attendu : Compilation réussie (fichier header uniquement).

---

### Tâche 2: Refonte de `SmartCamera` pour cibles dynamiques

**Fichiers concernés :**
- Modifier : `apps/astro_bazard/SmartCamera.hpp`
- Modifier : `apps/astro_bazard/SmartCamera.cpp`

**Étape 1 : Ajouter la méthode `setTarget`**
```cpp
// Dans SmartCamera.hpp, ajouter :
public:
    void setTarget(bb3d::Entity newTarget);
    bb3d::Entity getTarget() const { return m_currentTarget; }

private:
    bb3d::Entity m_currentTarget; // Remplace m_planet / m_ship hardcodés
```

**Étape 2 : Modifier le constructeur et l'update**
```cpp
// Dans SmartCamera.cpp
SmartCamera::SmartCamera(bb3d::Entity camera, bb3d::Entity initialTarget, float baseZoom)
    : m_camera(camera), m_currentTarget(initialTarget), m_baseZoom(baseZoom) {
}

void SmartCamera::setTarget(bb3d::Entity newTarget) {
    m_currentTarget = newTarget;
}

// Remplacer 'target' passé en paramètre de update() par m_currentTarget en interne.
```

**Étape 3 : Lancer la compilation**
Commande : `cmake --build build --target astro_bazard`
Attendu : Échec potentiel dans `main.cpp` qui utilise l'ancienne signature ou comportement.

---

### Tâche 3: Intégration du COMM-LINK dans la boucle de jeu

**Fichiers concernés :**
- Modifier : `apps/astro_bazard/main.cpp:Lignes 230-245`

**Étape 1 : Assigner le composant aux planètes et au vaisseau**
```cpp
// Dans main.cpp (lors de la création des planètes/vaisseau)
#include "CommRelay.hpp"

// Pour le vaisseau :
ship.add<astrobazard::CommRelayComponent>("Vaisseau Mère");

// Pour la planète :
planet.add<astrobazard::CommRelayComponent>("Prime Colony");
```

**Étape 2 : Logique de switch (Touche C)**
```cpp
// Dans la boucle Update (NativeScriptComponent)
static bool cWasPressed = false;
bool cIsPressed = input.isKeyPressed(bb3d::Key::C);

if (cIsPressed && !cWasPressed) {
    // Collecter toutes les entités avec CommRelayComponent
    auto view = enginePtr->getActiveScene()->getRegistry().view<astrobazard::CommRelayComponent>();
    std::vector<bb3d::Entity> targets;
    for (auto entityID : view) {
        targets.push_back(bb3d::Entity(entityID, enginePtr->getActiveScene().get()));
    }
    
    // Trouver la cible actuelle et passer à la suivante
    auto currentTarget = smartCamera->getTarget();
    auto it = std::find(targets.begin(), targets.end(), currentTarget);
    if (it != targets.end()) {
        it++;
        if (it == targets.end()) it = targets.begin();
        smartCamera->setTarget(*it);
        BB_CORE_INFO("COMM-LINK: Basculé sur {0}", (*it).get<astrobazard::CommRelayComponent>().name);
    }
}
cWasPressed = cIsPressed;
```

**Étape 3 : Lancer le jeu pour tester le comportement visuel**
Commande : `build\bin\astro_bazard.exe`
Attendu : Appuyer sur 'C' bascule la caméra fluide entre le vaisseau et les planètes.

**Étape 4 : Commit de la fonctionnalité COMM-LINK**
```bash
git add apps/astro_bazard/CommRelay.hpp apps/astro_bazard/SmartCamera.* apps/astro_bazard/main.cpp
git commit -m "feat(astro_bazard): implémentation du système de caméra COMM-LINK"
```
