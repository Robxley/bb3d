#pragma once

#include "bb3d/resource/Resource.hpp"
#include "bb3d/render/Mesh.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include <vector>
#include <string_view>

namespace bb3d {

class ResourceManager;

enum class ModelLoadPreset { PBR, CellShading };

/** @brief Configuration for the optimized and modular GLTF loader. */
struct ModelLoadConfig {
    ModelLoadPreset preset = ModelLoadPreset::PBR;
    
    bool loadAnimations = true;
    bool loadMaterials = true;
    bool loadPBRMaps = true;     ///< If false, only Albedo/Normal are loaded (Optimized for Toon).
    bool loadAlphaModes = true;  ///< If false, forces OPAQUE for all primitives.
    bool loadVertexColors = true;
    bool applyTransformations = true;
    
    glm::vec3 initialScale = {1.0f, 1.0f, 1.0f};
};

/**
 * @brief Complex asset representing a 3D model.
 * 
 * A model is a hierarchy of meshes (`Mesh`) and textures.
 * It supports:
 * - Loading via **fastgltf** (for .gltf/.glb).
 * - Loading via **tinyobjloader** (for .obj).
 * - Automatic bounding box calculation (AABB).
 * - Normalization (Automatic centering and scaling).
 */
class Model : public Resource {
public:
    /** 
     * @brief Loads a model from disk.
     * @param context Vulkan context for GPU transfer.
     * @param resourceManager Manager for the cache of textures linked to the model.
     * @param path Path to the asset.
     * @param config Optional modular loading configuration.
     */
    Model(VulkanContext& context, ResourceManager& resourceManager, std::string_view path, const ModelLoadConfig& config = ModelLoadConfig{});
    /** @brief Constructs an empty model for procedural generation. */
    Model(VulkanContext& context, ResourceManager& resourceManager);
    ~Model() override;

    /** @brief Records rendering commands for all meshes in the model. */
    void draw(vk::CommandBuffer commandBuffer);

    /** 
     * @brief Modifies vertex coordinates so that the model is centered 
     * and fits within a cube of specified dimensions.
     * @param targetSize Destination size (e.g., glm::vec3(1.0f) to fit in a 1m cube).
     * @return The applied center offset (to compensate the transform).
     */
    glm::vec3 normalize(const glm::vec3& targetSize = glm::vec3(1.0f));

    /** @brief Retrieves the spatial bounds of the model. */
    const AABB& getBounds() const { return m_bounds; }
    
    /** @brief Retrieves a texture loaded by the model. */
    Ref<Texture> getTexture(size_t index) const { return index < m_textures.size() ? m_textures[index] : nullptr; }

    /** @brief List of internal meshes in the model. */
    const std::vector<Ref<Mesh>>& getMeshes() const { return m_meshes; }
    
    /** @brief Adds a mesh to the model. */
    void addMesh(Ref<Mesh> mesh) {
        if (mesh) {
            m_meshes.push_back(mesh);
            m_bounds.extend(mesh->getBounds());
        }
    }

    /** @brief Releases RAM for all meshes in this model. */
    void releaseCPUData() {
        for (auto& mesh : m_meshes) mesh->releaseCPUData();
    }

private:
    void loadOBJ(std::string_view path);
    void loadGLTF(std::string_view path, const ModelLoadConfig& config);

    VulkanContext& m_context;
    ResourceManager& m_resourceManager;
    std::vector<Ref<Mesh>> m_meshes;
    std::vector<Ref<Texture>> m_textures;
    AABB m_bounds;
};

} // namespace bb3d
