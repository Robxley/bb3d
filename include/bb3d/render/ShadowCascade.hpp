#pragma once

#include <glm/glm.hpp>
#include <vector>

namespace bb3d {

class ShadowCascade {
public:
    /**
     * @brief Calcule les distances de découpe logarithmique/pratique pour les cascades.
     * @param cascadeCount Nombre de cascades.
     * @param nearPlane Distance du plan near de la caméra.
     * @param farPlane Distance du plan far de la caméra.
     * @param lambda Poids entre le gradient logarithmique et linéaire (généralement 0.5f à 0.75f).
     * @return Pointeur / Vecteur des distances pour chaque cascade.
     */
    static std::vector<float> calculateSplitDistances(uint32_t cascadeCount, float nearPlane, float farPlane, float lambda = 0.5f);

    /**
     * @brief Calcule la matrice View-Projection de la lumière pour une cascade donnée.
     * @param cameraProj Matrice de projection de la caméra (pour extraire le champ de vision, etc.).
     * @param cameraView Matrice de vue de la caméra (pour extraire la position/orientation).
     * @param lightDir Direction de la lumière principale (ex: soleil).
     * @param nearZ Début du sous-frustum (cascade Z near).
     * @param farZ Fin du sous-frustum (cascade Z far).
     * @param shadowMapRes Résolution de la shadow map (pour stabiliser/texel-snapper les ombres).
     * @return Matrice View-Projection alignée pour la lumière.
     */
    static glm::mat4 calculateLightSpaceMatrix(
        const glm::mat4& cameraProj, 
        const glm::mat4& cameraView, 
        const glm::vec3& lightDir, 
        float nearZ, 
        float farZ, 
        uint32_t shadowMapRes
    );
};

} // namespace bb3d
