#include "bb3d/render/Material.hpp"
#include "bb3d/render/UniformBuffer.hpp"
#include <array>

namespace bb3d {

Ref<Texture> Material::s_defaultWhite = nullptr;
Ref<Texture> Material::s_defaultBlack = nullptr;
Ref<Texture> Material::s_defaultNormal = nullptr;

void Material::InitDefaults(VulkanContext& context) {
    if (s_defaultWhite) return;
    std::array<std::byte, 4> white = { (std::byte)255, (std::byte)255, (std::byte)255, (std::byte)255 };
    s_defaultWhite = CreateRef<Texture>(context, std::span<const std::byte>(white), 1, 1, true);
    std::array<std::byte, 4> black = { (std::byte)0, (std::byte)0, (std::byte)0, (std::byte)255 };
    s_defaultBlack = CreateRef<Texture>(context, std::span<const std::byte>(black), 1, 1, true);
    std::array<std::byte, 4> normal = { (std::byte)128, (std::byte)128, (std::byte)255, (std::byte)255 };
    s_defaultNormal = CreateRef<Texture>(context, std::span<const std::byte>(normal), 1, 1, false);
}

void Material::Cleanup() { s_defaultWhite = nullptr; s_defaultBlack = nullptr; s_defaultNormal = nullptr; }

// --- PBRMaterial ---
PBRMaterial::PBRMaterial(VulkanContext& context) : Material(context) {
    InitDefaults(context);
    m_albedoMap = s_defaultWhite; m_normalMap = s_defaultNormal;
    m_metallicMap = s_defaultBlack; m_roughnessMap = s_defaultBlack;
    m_aoMap = s_defaultWhite; m_emissiveMap = s_defaultBlack;
    m_paramBuffer = CreateScope<UniformBuffer>(context, sizeof(PBRParameters));
}
PBRMaterial::~PBRMaterial() { m_paramBuffer.reset(); }
vk::DescriptorSetLayout PBRMaterial::CreateLayout(vk::Device device) {
    std::vector<vk::DescriptorSetLayoutBinding> b = {
        {0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment},
        {1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},
        {2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},
        {3, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},
        {4, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},
        {5, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},
        {6, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}
    };
    return device.createDescriptorSetLayout({ {}, (uint32_t)b.size(), b.data() });
}
vk::DescriptorSet PBRMaterial::getDescriptorSet(vk::DescriptorPool pool, vk::DescriptorSetLayout layout) {
    if (!m_set) { m_set = m_context.getDevice().allocateDescriptorSets({ pool, 1, &layout })[0]; m_dirty = true; }
    if (m_dirty) { updateDescriptorSet(); m_dirty = false; }
    return m_set;
}
void PBRMaterial::updateDescriptorSet() {
    m_paramBuffer->update(&m_parameters, sizeof(PBRParameters));
    vk::DescriptorBufferInfo bInfo(m_paramBuffer->getHandle(), 0, sizeof(PBRParameters));
    std::vector<vk::DescriptorImageInfo> iInfos = {
        {m_albedoMap->getSampler(), m_albedoMap->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal},
        {m_normalMap->getSampler(), m_normalMap->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal},
        {m_metallicMap->getSampler(), m_metallicMap->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal},
        {m_roughnessMap->getSampler(), m_roughnessMap->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal},
        {m_aoMap->getSampler(), m_aoMap->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal},
        {m_emissiveMap->getSampler(), m_emissiveMap->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal}
    };
    std::vector<vk::WriteDescriptorSet> writes;
    writes.push_back({ m_set, 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &bInfo });
    for(int i=0; i<6; ++i) writes.push_back({ m_set, (uint32_t)i+1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &iInfos[i] });
    m_context.getDevice().updateDescriptorSets(writes, {});
}

// --- UnlitMaterial ---
UnlitMaterial::UnlitMaterial(VulkanContext& context) : Material(context) { 
    InitDefaults(context); m_baseMap = s_defaultWhite; 
    m_paramBuffer = CreateScope<UniformBuffer>(context, sizeof(UnlitParameters));
}
UnlitMaterial::~UnlitMaterial() { m_paramBuffer.reset(); }
vk::DescriptorSetLayout UnlitMaterial::CreateLayout(vk::Device device) {
    std::vector<vk::DescriptorSetLayoutBinding> b = {
        {0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment},
        {1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}
    };
    return device.createDescriptorSetLayout({ {}, (uint32_t)b.size(), b.data() });
}
vk::DescriptorSet UnlitMaterial::getDescriptorSet(vk::DescriptorPool pool, vk::DescriptorSetLayout layout) {
    if (!m_set) { m_set = m_context.getDevice().allocateDescriptorSets({ pool, 1, &layout })[0]; m_dirty = true; }
    if (m_dirty) { updateDescriptorSet(); m_dirty = false; }
    return m_set;
}
void UnlitMaterial::updateDescriptorSet() {
    m_paramBuffer->update(&m_parameters, sizeof(UnlitParameters));
    vk::DescriptorBufferInfo bInfo(m_paramBuffer->getHandle(), 0, sizeof(UnlitParameters));
    vk::DescriptorImageInfo iInfo(m_baseMap->getSampler(), m_baseMap->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
    std::vector<vk::WriteDescriptorSet> writes = {
        { m_set, 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &bInfo },
        { m_set, 1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &iInfo }
    };
    m_context.getDevice().updateDescriptorSets(writes, {});
}

// --- ToonMaterial ---
ToonMaterial::ToonMaterial(VulkanContext& context) : Material(context) { 
    InitDefaults(context); m_baseMap = s_defaultWhite; 
    m_paramBuffer = CreateScope<UniformBuffer>(context, sizeof(ToonParameters));
}
ToonMaterial::~ToonMaterial() { m_paramBuffer.reset(); }
vk::DescriptorSetLayout ToonMaterial::CreateLayout(vk::Device device) {
    std::vector<vk::DescriptorSetLayoutBinding> b = {
        {0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment},
        {1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}
    };
    return device.createDescriptorSetLayout({ {}, (uint32_t)b.size(), b.data() });
}
vk::DescriptorSet ToonMaterial::getDescriptorSet(vk::DescriptorPool pool, vk::DescriptorSetLayout layout) {
    if (!m_set) { m_set = m_context.getDevice().allocateDescriptorSets({ pool, 1, &layout })[0]; m_dirty = true; }
    if (m_dirty) { updateDescriptorSet(); m_dirty = false; }
    return m_set;
}
void ToonMaterial::updateDescriptorSet() {
    m_paramBuffer->update(&m_parameters, sizeof(ToonParameters));
    vk::DescriptorBufferInfo bInfo(m_paramBuffer->getHandle(), 0, sizeof(ToonParameters));
    vk::DescriptorImageInfo iInfo(m_baseMap->getSampler(), m_baseMap->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
    std::vector<vk::WriteDescriptorSet> writes = {
        { m_set, 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &bInfo },
        { m_set, 1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &iInfo }
    };
    m_context.getDevice().updateDescriptorSets(writes, {});
}

// --- SkyboxMaterial ---
SkyboxMaterial::SkyboxMaterial(VulkanContext& context) : Material(context) { InitDefaults(context); }
SkyboxMaterial::~SkyboxMaterial() {}
vk::DescriptorSetLayout SkyboxMaterial::CreateLayout(vk::Device device) {
    vk::DescriptorSetLayoutBinding b(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment);
    return device.createDescriptorSetLayout({ {}, 1, &b });
}
vk::DescriptorSet SkyboxMaterial::getDescriptorSet(vk::DescriptorPool pool, vk::DescriptorSetLayout layout) {
    if (!m_set) { m_set = m_context.getDevice().allocateDescriptorSets({ pool, 1, &layout })[0]; m_dirty = true; }
    if (m_dirty) { updateDescriptorSet(); m_dirty = false; }
    return m_set;
}
void SkyboxMaterial::updateDescriptorSet() {
    if (!m_cubemap) return;
    vk::DescriptorImageInfo i(m_cubemap->getSampler(), m_cubemap->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
    vk::WriteDescriptorSet w(m_set, 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, &i);
    m_context.getDevice().updateDescriptorSets(1, &w, 0, nullptr);
}

// --- SkySphereMaterial ---
SkySphereMaterial::SkySphereMaterial(VulkanContext& context) : Material(context) { InitDefaults(context); m_texture = s_defaultWhite; }
SkySphereMaterial::~SkySphereMaterial() {}
vk::DescriptorSetLayout SkySphereMaterial::CreateLayout(vk::Device device) {
    vk::DescriptorSetLayoutBinding b(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment);
    return device.createDescriptorSetLayout({ {}, 1, &b });
}
vk::DescriptorSet SkySphereMaterial::getDescriptorSet(vk::DescriptorPool pool, vk::DescriptorSetLayout layout) {
    if (!m_set) { m_set = m_context.getDevice().allocateDescriptorSets({ pool, 1, &layout })[0]; m_dirty = true; }
    if (m_dirty) { updateDescriptorSet(); m_dirty = false; }
    return m_set;
}
void SkySphereMaterial::updateDescriptorSet() {
    vk::DescriptorImageInfo i(m_texture->getSampler(), m_texture->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
    vk::WriteDescriptorSet w(m_set, 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, &i);
    m_context.getDevice().updateDescriptorSets(1, &w, 0, nullptr);
}

} // namespace bb3d