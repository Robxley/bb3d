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

/**
 * @brief Classe de base pour les matériaux (Sets de paramètres shaders).
 * 
 * Un matériau définit comment une surface réagit à la lumière.
 * Il gère les Descriptor Sets (Vulkan) qui lient les textures et les uniform buffers aux shaders.
 */
class Material {
public:
    Material(VulkanContext& context) : m_context(context) {}
    virtual ~Material() = default;

    /** @brief Type de matériau (utilisé pour sélectionner le bon pipeline graphique). */
    virtual MaterialType getType() const = 0;

    /** 
     * @brief Récupère le Descriptor Set prêt à être utilisé par un Command Buffer.
     * Si les paramètres ont changé, le set est mis à jour avant d'être renvoyé.
     */
    virtual vk::DescriptorSet getDescriptorSet(vk::DescriptorPool pool, vk::DescriptorSetLayout layout) = 0;

    /** @brief Libère les ressources statiques (textures par défaut). */
    static void Cleanup(); 

protected:
    VulkanContext& m_context;
    // Textures de fallback partagées pour éviter les erreurs si une map est manquante.
    static Ref<Texture> s_defaultWhite;
    static Ref<Texture> s_defaultBlack;
    static Ref<Texture> s_defaultNormal;
    static void InitDefaults(VulkanContext& context);
};

/**
 * @brief Matériau standard PBR (Physically Based Rendering).
 * 
 * Workflow : Metallic-Roughness.
 * Maps supportées : Albedo, Normal, ORM (Occlusion/Roughness/Metallic), Emissive.
 */
class PBRMaterial : public Material {
public:
    PBRMaterial(VulkanContext& context);
    ~PBRMaterial() override;
    MaterialType getType() const override { return MaterialType::PBR; }

    /** @brief Définit la texture de couleur (Albedo). */
    void setAlbedoMap(Ref<Texture> texture) { Ref<Texture> t = texture ? texture : s_defaultWhite; if (m_albedoMap != t) { m_albedoMap = t; m_dirty = true; } }
    
    /** @brief Définit la texture de normales. */
    void setNormalMap(Ref<Texture> texture) { Ref<Texture> t = texture ? texture : s_defaultNormal; if (m_normalMap != t) { m_normalMap = t; m_dirty = true; } }
    
    /** @brief Définit la texture ORM (R: Occlusion, G: Roughness, B: Metallic). */
    void setORMMap(Ref<Texture> texture) { Ref<Texture> t = texture ? texture : s_defaultWhite; if (m_ormMap != t) { m_ormMap = t; m_dirty = true; } }
    
    void setEmissiveMap(Ref<Texture> texture) { Ref<Texture> t = texture ? texture : s_defaultBlack; if (m_emissiveMap != t) { m_emissiveMap = t; m_dirty = true; } }
    
    /** @brief Définit les facteurs scalaires PBR. */
    void setParameters(const PBRParameters& params) { m_parameters = params; m_dirty = true; }
    [[nodiscard]] const PBRParameters& getParameters() const { return m_parameters; }

    void setColor(const glm::vec3& color) { m_parameters.baseColorFactor = glm::vec4(color, 1.0f); m_dirty = true; }
    [[nodiscard]] glm::vec3 getColor() const { return glm::vec3(m_parameters.baseColorFactor); }
    
    vk::DescriptorSet getDescriptorSet(vk::DescriptorPool pool, vk::DescriptorSetLayout layout) override;
    
    /** @brief Crée le layout de descripteurs (Binding 0=Params, 1=Albedo, 2=Normal, 3=ORM, 4=Emissive). */
    static vk::DescriptorSetLayout CreateLayout(vk::Device device);

private:
    void updateDescriptorSet();
    Ref<Texture> m_albedoMap, m_normalMap, m_ormMap, m_emissiveMap;
    PBRParameters m_parameters;
    Scope<UniformBuffer> m_paramBuffer; ///< Buffer CPU->GPU pour les facteurs scalaires.
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
