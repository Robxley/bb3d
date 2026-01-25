#pragma once

#include "bb3d/core/Base.hpp"
#include "bb3d/render/Model.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <string>

#include "bb3d/scene/Camera.hpp"
#include <variant>

namespace bb3d {

/** @brief Composant pour identifier une entité par un nom. */
struct TagComponent {
    std::string tag;

    TagComponent() = default;
    TagComponent(const std::string& t) : tag(t) {}
};

/** @brief Composant gérant la position, rotation et l'échelle. */
struct TransformComponent {
    glm::vec3 translation = { 0.0f, 0.0f, 0.0f };
    glm::vec3 rotation = { 0.0f, 0.0f, 0.0f }; ///< Angles d'Euler.
    glm::vec3 scale = { 1.0f, 1.0f, 1.0f };

    TransformComponent() = default;

    /** @brief Calcule la matrice de transformation 4x4. */
    [[nodiscard]] inline glm::mat4 getTransform() const {
        return glm::translate(glm::mat4(1.0f), translation) * 
               glm::toMat4(glm::quat(rotation)) * 
               glm::scale(glm::mat4(1.0f), scale);
    }
};

/** @brief Composant pour l'affichage d'un maillage. */
struct MeshComponent {
    Ref<Mesh> mesh;
    glm::vec3 color = {1.0f, 1.0f, 1.0f};

    MeshComponent() = default;
    MeshComponent(Ref<Mesh> m) : mesh(m) {}
};

/** @brief Composant pour l'affichage d'un modèle complet (plusieurs meshes). */
struct ModelComponent {
    Ref<Model> model;

    ModelComponent() = default;
    ModelComponent(Ref<Model> m) : model(m) {}
};

/** @brief Composant Caméra. */
struct CameraComponent {
    Ref<Camera> camera;
    bool active = true;

    CameraComponent() = default;
    CameraComponent(Ref<Camera> c) : camera(c) {}
};

} // namespace bb3d