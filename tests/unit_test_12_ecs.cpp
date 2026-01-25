#include "bb3d/core/Log.hpp"
#include "bb3d/scene/Scene.hpp"
#include "bb3d/scene/Entity.hpp"
#include "bb3d/scene/Components.hpp"
#include <iostream>

int main() {
    bb3d::Log::Init();
    BB_CORE_INFO("Test Unitaire 12 : ECS & Scene");

    bb3d::Scene scene;

    // 1. Création d'entités
    auto ant = scene.createEntity("Ant");
    ant.get<bb3d::TransformComponent>().translation = { 1.0f, 2.0f, 3.0f };
    
    auto house = scene.createEntity("House");
    house.get<bb3d::TransformComponent>().translation = { 10.0f, 0.0f, -5.0f };

    // 2. Vérification des composants
    if (ant.has<bb3d::TagComponent>()) {
        BB_CORE_INFO("L'entité ant a bien un TagComponent : {}", ant.get<bb3d::TagComponent>().tag);
    }

    // 3. Itération (Système de logique simulé)
    BB_CORE_INFO("Parcours de toutes les entités avec un Transform...");
    auto view = scene.getRegistry().view<bb3d::TransformComponent, bb3d::TagComponent>();
    for (auto entity : view) {
        auto [transform, tag] = view.get<bb3d::TransformComponent, bb3d::TagComponent>(entity);
        BB_CORE_INFO("Entité '{}' à la position ({}, {}, {})", 
            tag.tag, transform.translation.x, transform.translation.y, transform.translation.z);
    }

    // 4. Suppression
    scene.destroyEntity(ant);
    BB_CORE_INFO("Entité ant supprimée.");

    return 0;
}
