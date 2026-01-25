#pragma once

#include "bb3d/resource/Resource.hpp"
#include "bb3d/render/Mesh.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include <vector>
#include <string_view>

namespace bb3d {

class ResourceManager;

/**
 * @brief Représente un modèle 3D complet composé de plusieurs maillages (Meshes).
 * 
 * Supporte le chargement de formats standard comme glTF et OBJ.
 */
class Model : public Resource {
public:
    /** @brief Charge un modèle depuis le disque. */
    Model(VulkanContext& context, ResourceManager& resourceManager, std::string_view path);
    ~Model() override;

    /** @brief Enregistre les commandes de rendu pour tous les maillages du modèle. */
    void draw(vk::CommandBuffer commandBuffer);
    
    /** @brief Récupère la boîte englobante alignée sur les axes du modèle. */
    [[nodiscard]] inline const AABB& getBounds() const { return m_bounds; }
    
    /** @brief Récupère une texture par son index. */
    [[nodiscard]] inline Ref<Texture> getTexture(size_t index) const {
        return (index < m_textures.size()) ? m_textures[index] : nullptr;
    }

private:
    void loadGLTF(std::string_view path);
    void loadOBJ(std::string_view path);

    VulkanContext& m_context;
    ResourceManager& m_resourceManager;
    std::vector<Scope<Mesh>> m_meshes;
    std::vector<Ref<Texture>> m_textures;
    AABB m_bounds;
};

} // namespace bb3d
