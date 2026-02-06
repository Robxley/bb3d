#pragma once

#include "bb3d/resource/Resource.hpp"
#include "bb3d/render/Mesh.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include <vector>
#include <string_view>

namespace bb3d {

class ResourceManager;

/**
 * @brief Asset complexe représentant un modèle 3D.
 * 
 * Un modèle est une hiérarchie de maillages (`Mesh`) et de textures.
 * Il supporte :
 * - Le chargement via **fastgltf** (pour .gltf/.glb).
 * - Le chargement via **tinyobjloader** (pour .obj).
 * - Le calcul automatique de boîte englobante (AABB).
 * - La normalisation (Recentrage et mise à l'échelle automatique).
 */
class Model : public Resource {
public:
    /** 
     * @brief Charge un modèle depuis le disque.
     * @param context Contexte Vulkan pour le transfert GPU.
     * @param resourceManager Manager pour le cache des textures liées au modèle.
     * @param path Chemin vers l'asset.
     */
    Model(VulkanContext& context, ResourceManager& resourceManager, std::string_view path);
    ~Model() override;

    /** @brief Enregistre les commandes de rendu pour tous les maillages du modèle. */
    void draw(vk::CommandBuffer commandBuffer);

    /** 
     * @brief Modifie les coordonnées des sommets pour que le modèle soit centré 
     * et tienne dans un cube de dimensions spécifiées.
     * @param targetSize Taille de destination (ex: glm::vec3(1.0f) pour tenir dans 1m cube).
     */
    void normalize(const glm::vec3& targetSize = glm::vec3(1.0f));

    /** @brief Récupère les limites spatiales du modèle. */
    const AABB& getBounds() const { return m_bounds; }
    
    /** @brief Récupère une texture chargée par le modèle. */
    Ref<Texture> getTexture(size_t index) const { return index < m_textures.size() ? m_textures[index] : nullptr; }

    /** @brief Liste des maillages internes du modèle. */
    const std::vector<Scope<Mesh>>& getMeshes() const { return m_meshes; }

    /** @brief Libère la RAM pour tous les maillages de ce modèle. */
    void releaseCPUData() {
        for (auto& mesh : m_meshes) mesh->releaseCPUData();
    }

private:
    void loadOBJ(std::string_view path);
    void loadGLTF(std::string_view path);

    VulkanContext& m_context;
    ResourceManager& m_resourceManager;
    std::vector<Scope<Mesh>> m_meshes;
    std::vector<Ref<Texture>> m_textures;
    AABB m_bounds;
};

} // namespace bb3d
