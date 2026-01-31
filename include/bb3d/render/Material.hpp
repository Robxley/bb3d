#pragma once

#include "bb3d/core/Base.hpp"
#include "bb3d/render/Texture.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include <glm/glm.hpp>

namespace bb3d {

enum class MaterialType { PBR, Unlit, Toon };

struct PBRParameters {
    glm::vec4 baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    float normalScale = 1.0f;
    float occlusionStrength = 1.0f;
};

class Material {
public:
    Material(VulkanContext& context) : m_context(context) {}
    virtual ~Material() = default;

    virtual MaterialType getType() const = 0;
    virtual vk::DescriptorSetLayout getDescriptorSetLayout(vk::Device device) = 0;
    virtual vk::DescriptorSet getDescriptorSet(vk::DescriptorPool pool) = 0;

    static void Cleanup(); // Nettoyage des ressources statiques

protected:
    VulkanContext& m_context;
    
    // Textures par défaut partagées
    static Ref<Texture> s_defaultWhite;
    static Ref<Texture> s_defaultBlack;
    static Ref<Texture> s_defaultNormal;

    static void InitDefaults(VulkanContext& context);
};

class PBRMaterial : public Material {
public:
    PBRMaterial(VulkanContext& context);
    ~PBRMaterial() = default;

    MaterialType getType() const override { return MaterialType::PBR; }

    void setAlbedoMap(Ref<Texture> texture) { m_albedoMap = texture; m_dirty = true; }
    void setNormalMap(Ref<Texture> texture) { m_normalMap = texture; m_dirty = true; }
    void setMetallicMap(Ref<Texture> texture) { m_metallicMap = texture; m_dirty = true; }
    void setRoughnessMap(Ref<Texture> texture) { m_roughnessMap = texture; m_dirty = true; }
    void setAOMap(Ref<Texture> texture) { m_aoMap = texture; m_dirty = true; }
    void setEmissiveMap(Ref<Texture> texture) { m_emissiveMap = texture; m_dirty = true; }
    void setParameters(const PBRParameters& params) { m_parameters = params; }

    vk::DescriptorSetLayout getDescriptorSetLayout(vk::Device device) override;
    vk::DescriptorSet getDescriptorSet(vk::DescriptorPool pool) override;

private:
    void updateDescriptorSet();

    Ref<Texture> m_albedoMap;
    Ref<Texture> m_normalMap;
    Ref<Texture> m_metallicMap;
    Ref<Texture> m_roughnessMap;
    Ref<Texture> m_aoMap;
    Ref<Texture> m_emissiveMap;
    PBRParameters m_parameters;

    vk::DescriptorSetLayout m_layout = nullptr;
    vk::DescriptorSet m_set = nullptr;
    bool m_dirty = true;
};

class UnlitMaterial : public Material {
public:
    UnlitMaterial(VulkanContext& context);
    MaterialType getType() const override { return MaterialType::Unlit; }

    void setBaseMap(Ref<Texture> texture) { m_baseMap = texture; m_dirty = true; }
    void setColor(const glm::vec3& color) { m_color = color; }

    vk::DescriptorSetLayout getDescriptorSetLayout(vk::Device device) override;
    vk::DescriptorSet getDescriptorSet(vk::DescriptorPool pool) override;

private:
    void updateDescriptorSet();
    Ref<Texture> m_baseMap;
    glm::vec3 m_color = {1.0f, 1.0f, 1.0f};
    vk::DescriptorSetLayout m_layout = nullptr;
    vk::DescriptorSet m_set = nullptr;
    bool m_dirty = true;
};

class ToonMaterial : public Material {
public:
    ToonMaterial(VulkanContext& context);
    MaterialType getType() const override { return MaterialType::Toon; }

    void setBaseMap(Ref<Texture> texture) { m_baseMap = texture; m_dirty = true; }

    vk::DescriptorSetLayout getDescriptorSetLayout(vk::Device device) override;
    vk::DescriptorSet getDescriptorSet(vk::DescriptorPool pool) override;

private:
    void updateDescriptorSet();
    Ref<Texture> m_baseMap;
    vk::DescriptorSetLayout m_layout = nullptr;
    vk::DescriptorSet m_set = nullptr;
    bool m_dirty = true;
};

} // namespace bb3d