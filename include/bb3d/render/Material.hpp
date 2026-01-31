#pragma once

#include "bb3d/core/Base.hpp"
#include "bb3d/render/Texture.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include <glm/glm.hpp>

namespace bb3d {

struct PBRParameters {
    glm::vec4 baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    float normalScale = 1.0f;
    float occlusionStrength = 1.0f;
    // Padding pour alignement std140 si utilisé en UBO
};

class Material {
public:
    Material(VulkanContext& context);
    ~Material();

    // Setters
    void setAlbedoMap(Ref<Texture> texture) { m_albedoMap = texture; m_dirty = true; }
    void setNormalMap(Ref<Texture> texture) { m_normalMap = texture; m_dirty = true; }
    void setMetallicMap(Ref<Texture> texture) { m_metallicMap = texture; m_dirty = true; }
    void setRoughnessMap(Ref<Texture> texture) { m_roughnessMap = texture; m_dirty = true; }
    void setAOMap(Ref<Texture> texture) { m_aoMap = texture; m_dirty = true; }
    void setEmissiveMap(Ref<Texture> texture) { m_emissiveMap = texture; m_dirty = true; }
    
    void setParameters(const PBRParameters& params) { m_parameters = params; }

    // Getters
    Ref<Texture> getAlbedoMap() const { return m_albedoMap; }
    Ref<Texture> getNormalMap() const { return m_normalMap; }
    
    // Vulkan Resources
    vk::DescriptorSet getDescriptorSet(vk::DescriptorSetLayout layout, vk::DescriptorPool pool);

    static void Cleanup();

private:
    void updateDescriptorSet(vk::DescriptorSet set);

    VulkanContext& m_context;
    
    Ref<Texture> m_albedoMap;
    Ref<Texture> m_normalMap;
    Ref<Texture> m_metallicMap;
    Ref<Texture> m_roughnessMap;
    Ref<Texture> m_aoMap;
    Ref<Texture> m_emissiveMap;

    PBRParameters m_parameters;

    vk::DescriptorSet m_descriptorSet = nullptr;
    bool m_dirty = true;
    
    // Texture par défaut (blanche) pour les slots vides
    static Ref<Texture> s_defaultWhite;
    static Ref<Texture> s_defaultBlack; // (0,0,0,1)
    static Ref<Texture> s_defaultNormal; // (0.5, 0.5, 1.0)
};

} // namespace bb3d