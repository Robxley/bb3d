#pragma once

#include "bb3d/core/Base.hpp"
#include "bb3d/render/Texture.hpp"
#include "bb3d/render/UniformBuffer.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include <glm/glm.hpp>

namespace bb3d {

enum class MaterialType { PBR, Unlit, Toon, Skybox, SkySphere, Highlight };

enum class AlphaMode { Opaque = 0, Mask, Blend };

struct PBRParameters {
    glm::vec4 baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec3 emissiveFactor = {0.0f, 0.0f, 0.0f};
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    float normalScale = 1.0f;
    float occlusionStrength = 1.0f;
    float alphaCutoff = 0.5f;
    AlphaMode alphaMode = AlphaMode::Opaque;
    bool doubleSided = false;
};

struct UnlitParameters {
    glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
};

struct ToonParameters {
    glm::vec4 baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec3 emissiveFactor = {0.0f, 0.0f, 0.0f};
    float alphaCutoff = 0.5f;
    AlphaMode alphaMode = AlphaMode::Opaque;
    bool doubleSided = false;
};

/**
 * @brief Base class for materials (Shader parameter sets).
 * 
 * A material defines how a surface reacts to light.
 * It manages Descriptor Sets (Vulkan) that bind textures and uniform buffers to shaders.
 */
class Material {
public:
    Material(VulkanContext& context) : m_context(context) {}
    virtual ~Material() = default;

    /** @brief Material type (used to select the correct graphics pipeline). */
    virtual MaterialType getType() const = 0;

    /** 
     * @brief Retrieves the Descriptor Set ready to be used by a Command Buffer.
     * If parameters have changed, the set is updated before being returned.
     */
    virtual vk::DescriptorSet getDescriptorSet(vk::DescriptorPool pool, vk::DescriptorSetLayout layout) = 0;

    /** @brief Releases static resources (default textures). */
    static void Cleanup(); 

    /** @brief Sets the current frame index for descriptor buffering. */
    static void SetCurrentFrame(uint32_t frame) { s_currentFrame = frame; }

protected:
    VulkanContext& m_context;
    static inline uint32_t s_currentFrame = 0;
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

    // Shared fallback textures to avoid errors if a map is missing.
    static Ref<Texture> s_defaultWhite;
    static Ref<Texture> s_defaultBlack;
    static Ref<Texture> s_defaultNormal;
    static void InitDefaults(VulkanContext& context);
};

/**
 * @brief Standard PBR (Physically Based Rendering) material.
 * 
 * Workflow: Metallic-Roughness.
 * Supported maps: Albedo, Normal, ORM (Occlusion/Roughness/Metallic), Emissive.
 */
class PBRMaterial : public Material {
public:
    PBRMaterial(VulkanContext& context);
    ~PBRMaterial() override;
    MaterialType getType() const override { return MaterialType::PBR; }

    /** @brief Sets the color texture (Albedo). */
    void setAlbedoMap(Ref<Texture> texture) { Ref<Texture> t = texture ? texture : s_defaultWhite; if (m_albedoMap != t) { m_albedoMap = t; m_dirty.fill(true); } }
    
    /** @brief Sets the normal texture. */
    void setNormalMap(Ref<Texture> texture) { Ref<Texture> t = texture ? texture : s_defaultNormal; if (m_normalMap != t) { m_normalMap = t; m_dirty.fill(true); } }
    
    /** @brief Sets the ORM texture (R: Occlusion, G: Roughness, B: Metallic). */
    void setORMMap(Ref<Texture> texture) { Ref<Texture> t = texture ? texture : s_defaultWhite; if (m_ormMap != t) { m_ormMap = t; m_dirty.fill(true); } }
    
    void setEmissiveMap(Ref<Texture> texture) { Ref<Texture> t = texture ? texture : s_defaultBlack; if (m_emissiveMap != t) { m_emissiveMap = t; m_dirty.fill(true); } }
    
    /** @brief Sets PBR scalar factors. */
    void setParameters(const PBRParameters& params) { m_parameters = params; m_dirty.fill(true); }
    [[nodiscard]] const PBRParameters& getParameters() const { return m_parameters; }

    void setColor(const glm::vec3& color) { m_parameters.baseColorFactor = glm::vec4(color, 1.0f); m_dirty.fill(true); }
    [[nodiscard]] glm::vec3 getColor() const { return glm::vec3(m_parameters.baseColorFactor); }
    
    vk::DescriptorSet getDescriptorSet(vk::DescriptorPool pool, vk::DescriptorSetLayout layout) override;
    
    /** @brief Creates the descriptor layout (Binding 0=Params, 1=Albedo, 2=Normal, 3=ORM, 4=Emissive). */
    static vk::DescriptorSetLayout CreateLayout(vk::Device device);

private:
    void updateDescriptorSet(uint32_t frame);
    Ref<Texture> m_albedoMap, m_normalMap, m_ormMap, m_emissiveMap;
    PBRParameters m_parameters;
    std::array<Scope<UniformBuffer>, MAX_FRAMES_IN_FLIGHT> m_paramBuffers;
    std::array<vk::DescriptorSet, MAX_FRAMES_IN_FLIGHT> m_sets = {nullptr, nullptr, nullptr};
    std::array<bool, MAX_FRAMES_IN_FLIGHT> m_dirty = {true, true, true};
};

class UnlitMaterial : public Material {
public:
    UnlitMaterial(VulkanContext& context);
    ~UnlitMaterial() override;
    MaterialType getType() const override { return MaterialType::Unlit; }
    void setBaseMap(Ref<Texture> texture) { if (m_baseMap != texture) { m_baseMap = texture; m_dirty.fill(true); } }
    void setColor(const glm::vec3& color) { m_parameters.color = glm::vec4(color, 1.0f); m_dirty.fill(true); }
    vk::DescriptorSet getDescriptorSet(vk::DescriptorPool pool, vk::DescriptorSetLayout layout) override;
    static vk::DescriptorSetLayout CreateLayout(vk::Device device);
private:
    void updateDescriptorSet(uint32_t frame);
    Ref<Texture> m_baseMap;
    UnlitParameters m_parameters;
    std::array<Scope<UniformBuffer>, MAX_FRAMES_IN_FLIGHT> m_paramBuffers;
    std::array<vk::DescriptorSet, MAX_FRAMES_IN_FLIGHT> m_sets = {nullptr, nullptr, nullptr};
    std::array<bool, MAX_FRAMES_IN_FLIGHT> m_dirty = {true, true, true};
};

class ToonMaterial : public Material {
public:
    ToonMaterial(VulkanContext& context);
    ~ToonMaterial() override;
    MaterialType getType() const override { return MaterialType::Toon; }
    void setBaseMap(Ref<Texture> texture) { if (m_baseMap != texture) { m_baseMap = texture; m_dirty.fill(true); } }
    vk::DescriptorSet getDescriptorSet(vk::DescriptorPool pool, vk::DescriptorSetLayout layout) override;
    static vk::DescriptorSetLayout CreateLayout(vk::Device device);
private:
    void updateDescriptorSet(uint32_t frame);
    Ref<Texture> m_baseMap;
    ToonParameters m_parameters;
    std::array<Scope<UniformBuffer>, MAX_FRAMES_IN_FLIGHT> m_paramBuffers;
    std::array<vk::DescriptorSet, MAX_FRAMES_IN_FLIGHT> m_sets = {nullptr, nullptr, nullptr};
    std::array<bool, MAX_FRAMES_IN_FLIGHT> m_dirty = {true, true, true};
};

class SkyboxMaterial : public Material {
public:
    SkyboxMaterial(VulkanContext& context);
    ~SkyboxMaterial() override;
    MaterialType getType() const override { return MaterialType::Skybox; }
    void setCubemap(Ref<Texture> texture) { if (m_cubemap != texture) { m_cubemap = texture; m_dirty.fill(true); } }
    vk::DescriptorSet getDescriptorSet(vk::DescriptorPool pool, vk::DescriptorSetLayout layout) override;
    static vk::DescriptorSetLayout CreateLayout(vk::Device device);
private:
    void updateDescriptorSet(uint32_t frame);
    Ref<Texture> m_cubemap;
    std::array<vk::DescriptorSet, MAX_FRAMES_IN_FLIGHT> m_sets = {nullptr, nullptr, nullptr};
    std::array<bool, MAX_FRAMES_IN_FLIGHT> m_dirty = {true, true, true};
};

class SkySphereMaterial : public Material {
public:
    SkySphereMaterial(VulkanContext& context);
    ~SkySphereMaterial() override;
    MaterialType getType() const override { return MaterialType::SkySphere; }
    void setTexture(Ref<Texture> texture) { if (m_texture != texture) { m_texture = texture; m_dirty.fill(true); } }
    vk::DescriptorSet getDescriptorSet(vk::DescriptorPool pool, vk::DescriptorSetLayout layout) override;
    static vk::DescriptorSetLayout CreateLayout(vk::Device device);
private:
    void updateDescriptorSet(uint32_t frame);
    Ref<Texture> m_texture;
    std::array<vk::DescriptorSet, MAX_FRAMES_IN_FLIGHT> m_sets = {nullptr, nullptr, nullptr};
    std::array<bool, MAX_FRAMES_IN_FLIGHT> m_dirty = {true, true, true};
};

} // namespace bb3d
