#pragma once

#include "bb3d/core/Base.hpp"
#include "bb3d/render/Texture.hpp"
#include "bb3d/render/UniformBuffer.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include <glm/glm.hpp>

namespace bb3d {

enum class MaterialType { PBR, Unlit, Toon, Skybox, SkySphere };

struct PBRParameters {
    glm::vec4 baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    float normalScale = 1.0f;
    float occlusionStrength = 1.0f;
};

struct UnlitParameters {
    glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
};

struct ToonParameters {
    glm::vec4 baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
};

class Material {
public:
    Material(VulkanContext& context) : m_context(context) {}
    virtual ~Material() = default;

    virtual MaterialType getType() const = 0;
    virtual vk::DescriptorSet getDescriptorSet(vk::DescriptorPool pool, vk::DescriptorSetLayout layout) = 0;

    static void Cleanup(); 

protected:
    VulkanContext& m_context;
    static Ref<Texture> s_defaultWhite;
    static Ref<Texture> s_defaultBlack;
    static Ref<Texture> s_defaultNormal;
    static void InitDefaults(VulkanContext& context);
};

class PBRMaterial : public Material {
public:
    PBRMaterial(VulkanContext& context);
    ~PBRMaterial() override;
    MaterialType getType() const override { return MaterialType::PBR; }
    void setAlbedoMap(Ref<Texture> texture) { Ref<Texture> t = texture ? texture : s_defaultWhite; if (m_albedoMap != t) { m_albedoMap = t; m_dirty = true; } }
    void setNormalMap(Ref<Texture> texture) { Ref<Texture> t = texture ? texture : s_defaultNormal; if (m_normalMap != t) { m_normalMap = t; m_dirty = true; } }
    void setORMMap(Ref<Texture> texture) { Ref<Texture> t = texture ? texture : s_defaultWhite; if (m_ormMap != t) { m_ormMap = t; m_dirty = true; } }
    void setEmissiveMap(Ref<Texture> texture) { Ref<Texture> t = texture ? texture : s_defaultBlack; if (m_emissiveMap != t) { m_emissiveMap = t; m_dirty = true; } }
    void setParameters(const PBRParameters& params) { m_parameters = params; m_dirty = true; }
    
    vk::DescriptorSet getDescriptorSet(vk::DescriptorPool pool, vk::DescriptorSetLayout layout) override;
    static vk::DescriptorSetLayout CreateLayout(vk::Device device);

private:
    void updateDescriptorSet();
    Ref<Texture> m_albedoMap, m_normalMap, m_ormMap, m_emissiveMap;
    PBRParameters m_parameters;
    Scope<UniformBuffer> m_paramBuffer;
    vk::DescriptorSet m_set = nullptr;
    bool m_dirty = true;
};

class UnlitMaterial : public Material {
public:
    UnlitMaterial(VulkanContext& context);
    ~UnlitMaterial() override;
    MaterialType getType() const override { return MaterialType::Unlit; }
    void setBaseMap(Ref<Texture> texture) { if (m_baseMap != texture) { m_baseMap = texture; m_dirty = true; } }
    void setColor(const glm::vec3& color) { m_parameters.color = glm::vec4(color, 1.0f); m_dirty = true; }
    vk::DescriptorSet getDescriptorSet(vk::DescriptorPool pool, vk::DescriptorSetLayout layout) override;
    static vk::DescriptorSetLayout CreateLayout(vk::Device device);
private:
    void updateDescriptorSet();
    Ref<Texture> m_baseMap;
    UnlitParameters m_parameters;
    Scope<UniformBuffer> m_paramBuffer;
    vk::DescriptorSet m_set = nullptr;
    bool m_dirty = true;
};

class ToonMaterial : public Material {
public:
    ToonMaterial(VulkanContext& context);
    ~ToonMaterial() override;
    MaterialType getType() const override { return MaterialType::Toon; }
    void setBaseMap(Ref<Texture> texture) { if (m_baseMap != texture) { m_baseMap = texture; m_dirty = true; } }
    vk::DescriptorSet getDescriptorSet(vk::DescriptorPool pool, vk::DescriptorSetLayout layout) override;
    static vk::DescriptorSetLayout CreateLayout(vk::Device device);
private:
    void updateDescriptorSet();
    Ref<Texture> m_baseMap;
    ToonParameters m_parameters;
    Scope<UniformBuffer> m_paramBuffer;
    vk::DescriptorSet m_set = nullptr;
    bool m_dirty = true;
};

class SkyboxMaterial : public Material {
public:
    SkyboxMaterial(VulkanContext& context);
    ~SkyboxMaterial() override;
    MaterialType getType() const override { return MaterialType::Skybox; }
    void setCubemap(Ref<Texture> texture) { if (m_cubemap != texture) { m_cubemap = texture; m_dirty = true; } }
    vk::DescriptorSet getDescriptorSet(vk::DescriptorPool pool, vk::DescriptorSetLayout layout) override;
    static vk::DescriptorSetLayout CreateLayout(vk::Device device);
private:
    void updateDescriptorSet();
    Ref<Texture> m_cubemap;
    vk::DescriptorSet m_set = nullptr;
    bool m_dirty = true;
};

class SkySphereMaterial : public Material {
public:
    SkySphereMaterial(VulkanContext& context);
    ~SkySphereMaterial() override;
    MaterialType getType() const override { return MaterialType::SkySphere; }
    void setTexture(Ref<Texture> texture) { if (m_texture != texture) { m_texture = texture; m_dirty = true; } }
    vk::DescriptorSet getDescriptorSet(vk::DescriptorPool pool, vk::DescriptorSetLayout layout) override;
    static vk::DescriptorSetLayout CreateLayout(vk::Device device);
private:
    void updateDescriptorSet();
    Ref<Texture> m_texture;
    vk::DescriptorSet m_set = nullptr;
    bool m_dirty = true;
};

} // namespace bb3d
