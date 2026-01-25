#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace bb3d {

/** @brief Données de la caméra envoyées aux shaders via Uniform Buffers. */
struct CameraUniform {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewProj; ///< Matrices combinées pré-calculées.
    glm::vec3 position;
};

/**
 * @brief Classe de base abstraite pour toutes les caméras du moteur.
 */
class Camera {
public:
    /**
     * @brief Initialise une caméra perspective de base.
     * @param fov Champ de vision (en degrés).
     * @param aspect Rapport d'aspect (width/height).
     * @param near Plan de coupe proche.
     * @param far Plan de coupe lointain.
     */
    Camera(float fov, float aspect, float near, float far);
    virtual ~Camera() = default;

    /** @brief Met à jour la logique de la caméra (ex: interpolation). */
    virtual void update(float deltaTime) = 0;

    /** @name Getters
     * @{
     */
    [[nodiscard]] inline const glm::mat4& getViewMatrix() const { return m_view; }
    [[nodiscard]] inline const glm::mat4& getProjectionMatrix() const { return m_proj; }
    [[nodiscard]] inline const glm::vec3& getPosition() const { return m_position; }
    /** @} */

    /** @brief Définit la position de la caméra. */
    virtual void setPosition(const glm::vec3& position) { m_position = position; }

    /** @brief Oriente la caméra vers une cible spécifique. */
    virtual void lookAt(const glm::vec3& target);

    /** @brief Met à jour les paramètres de projection perspective. */
    void setPerspective(float fov, float aspect, float near, float far);

    /** @brief Récupère les données formatées pour un Uniform Buffer. */
    [[nodiscard]] CameraUniform getUniformData() const;

protected:
    glm::vec3 m_position{0.0f, 0.0f, 5.0f};
    glm::mat4 m_view{1.0f};
    glm::mat4 m_proj{1.0f};
};

} // namespace bb3d
