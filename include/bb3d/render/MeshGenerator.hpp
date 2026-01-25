#pragma once

#include "bb3d/render/Mesh.hpp"
#include <glm/glm.hpp>
#include <vector>

namespace bb3d {

/**
 * @brief Utilitaire pour générer des maillages de formes primitives simples.
 */
class MeshGenerator {
public:
    /**
     * @brief Génère un cube centré à l'origine.
     * @param context Contexte Vulkan pour la création des buffers.
     * @param size Taille du cube (longueur d'un côté).
     * @param color Couleur appliquée à tous les sommets.
     * @return Scope<Mesh> Le maillage généré.
     */
    static Scope<Mesh> createCube(VulkanContext& context, float size = 1.0f, const glm::vec3& color = {1.0f, 1.0f, 1.0f});

    /**
     * @brief Génère une sphère UV.
     * @param context Contexte Vulkan.
     * @param radius Rayon de la sphère.
     * @param segments Nombre de segments horizontaux et verticaux.
     * @param color Couleur de base.
     * @return Scope<Mesh> Le maillage généré.
     */
    static Scope<Mesh> createSphere(VulkanContext& context, float radius = 0.5f, uint32_t segments = 32, const glm::vec3& color = {1.0f, 1.0f, 1.0f});

    /**
     * @brief Génère un plan horizontal (XZ) avec un motif de damier coloré.
     * @param context Contexte Vulkan.
     * @param size Taille totale du plan.
     * @param subdivisions Nombre de carreaux par côté.
     * @param color1 Première couleur du damier.
     * @param color2 Deuxième couleur du damier.
     * @return Scope<Mesh> Le maillage généré.
     */
    static Scope<Mesh> createCheckerboardPlane(VulkanContext& context, float size = 10.0f, uint32_t subdivisions = 10, const glm::vec3& color1 = {0.2f, 0.2f, 0.2f}, const glm::vec3& color2 = {0.8f, 0.8f, 0.8f});
};

} // namespace bb3d
