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

void Material::Cleanup() { 
    if (s_defaultWhite) s_defaultWhite.reset();
    if (s_defaultBlack) s_defaultBlack.reset();
    if (s_defaultNormal) s_defaultNormal.reset();
    s_defaultWhite = nullptr; 
    s_defaultBlack = nullptr; 
    s_defaultNormal = nullptr; 
}

// --- PBRMaterial ---
PBRMaterial::PBRMaterial(VulkanContext& context) : Material(context) {
    InitDefaults(context);
    m_albedoMap = s_defaultWhite; m_normalMap = s_defaultNormal;
    m_ormMap = s_defaultWhite; // Occlusion (R)=1, Roughness (G)=1, Metallic (B)=1
    m_emissiveMap = s_defaultBlack;
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_paramBuffers[i] = CreateScope<UniformBuffer>(context, sizeof(PBRParameters));
    }
}
PBRMaterial::~PBRMaterial() { 
    for (auto& buffer : m_paramBuffers) buffer.reset(); 
}
vk::DescriptorSetLayout PBRMaterial::CreateLayout(vk::Device device) {
    std::vector<vk::DescriptorSetLayoutBinding> b = {
        {0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment},
        {1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}, // Albedo
        {2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}, // Normal
        {3, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}, // ORM
        {4, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}  // Emissive
    };
    return device.createDescriptorSetLayout({ {}, (uint32_t)b.size(), b.data() });
}
vk::DescriptorSet PBRMaterial::getDescriptorSet(vk::DescriptorPool pool, vk::DescriptorSetLayout layout) {
    uint32_t frame = s_currentFrame;
    if (!m_sets[frame]) { 
        m_sets[frame] = m_context.getDevice().allocateDescriptorSets({ pool, 1, &layout })[0]; 
        m_dirty[frame] = true; 
    }

    // Si une des textures n'est pas prête, on reste en dirty pour réessayer la frame suivante
    if ((m_albedoMap && !m_albedoMap->isReady()) || 
        (m_normalMap && !m_normalMap->isReady()) || 
        (m_ormMap && !m_ormMap->isReady()) || 
        (m_emissiveMap && !m_emissiveMap->isReady())) {
        m_dirty[frame] = true;
    }

    if (m_dirty[frame]) { 
        updateDescriptorSet(frame); 
        m_dirty[frame] = false; 
    }
    return m_sets[frame];
}
void PBRMaterial::updateDescriptorSet(uint32_t frame) {
    m_paramBuffers[frame]->update(&m_parameters, sizeof(PBRParameters));
    vk::DescriptorBufferInfo bInfo(m_paramBuffers[frame]->getHandle(), 0, sizeof(PBRParameters));
    
    auto getReadyTexture = [](Ref<Texture> tex, Ref<Texture> fallback) {
        return (tex && tex->isReady()) ? tex : fallback;
    };

    std::vector<vk::DescriptorImageInfo> iInfos = {
        {getReadyTexture(m_albedoMap, s_defaultWhite)->getSampler(), getReadyTexture(m_albedoMap, s_defaultWhite)->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal},
        {getReadyTexture(m_normalMap, s_defaultNormal)->getSampler(), getReadyTexture(m_normalMap, s_defaultNormal)->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal},
        {getReadyTexture(m_ormMap, s_defaultWhite)->getSampler(), getReadyTexture(m_ormMap, s_defaultWhite)->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal},
        {getReadyTexture(m_emissiveMap, s_defaultBlack)->getSampler(), getReadyTexture(m_emissiveMap, s_defaultBlack)->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal}
    };
    std::vector<vk::WriteDescriptorSet> writes;
    writes.push_back({ m_sets[frame], 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &bInfo });
    for(int i=0; i<4; ++i) writes.push_back({ m_sets[frame], (uint32_t)i+1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &iInfos[i] });
    m_context.getDevice().updateDescriptorSets(writes, {});
}

// --- UnlitMaterial ---
UnlitMaterial::UnlitMaterial(VulkanContext& context) : Material(context) { 
    InitDefaults(context); m_baseMap = s_defaultWhite; 
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_paramBuffers[i] = CreateScope<UniformBuffer>(context, sizeof(UnlitParameters));
    }
}
UnlitMaterial::~UnlitMaterial() { 
    for (auto& buffer : m_paramBuffers) buffer.reset(); 
}
vk::DescriptorSetLayout UnlitMaterial::CreateLayout(vk::Device device) {
    std::vector<vk::DescriptorSetLayoutBinding> b = {
        {0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment},
        {1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}
    };
    return device.createDescriptorSetLayout({ {}, (uint32_t)b.size(), b.data() });
}
vk::DescriptorSet UnlitMaterial::getDescriptorSet(vk::DescriptorPool pool, vk::DescriptorSetLayout layout) {
    uint32_t frame = s_currentFrame;
    if (!m_sets[frame]) { 
        m_sets[frame] = m_context.getDevice().allocateDescriptorSets({ pool, 1, &layout })[0]; 
        m_dirty[frame] = true; 
    }
    
    // Si la texture n'est pas prête, on reste en dirty pour réessayer plus tard sans bloquer
    if (m_baseMap && !m_baseMap->isReady()) m_dirty[frame] = true;

    if (m_dirty[frame]) { updateDescriptorSet(frame); m_dirty[frame] = false; }
    return m_sets[frame];
}
void UnlitMaterial::updateDescriptorSet(uint32_t frame) {
    m_paramBuffers[frame]->update(&m_parameters, sizeof(UnlitParameters));
    vk::DescriptorBufferInfo bInfo(m_paramBuffers[frame]->getHandle(), 0, sizeof(UnlitParameters));
    
    Ref<Texture> tex = (m_baseMap && m_baseMap->isReady()) ? m_baseMap : s_defaultWhite;
    vk::DescriptorImageInfo iInfo(tex->getSampler(), tex->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
    
    std::vector<vk::WriteDescriptorSet> writes = {
        { m_sets[frame], 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &bInfo },
        { m_sets[frame], 1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &iInfo }
    };
    m_context.getDevice().updateDescriptorSets(writes, {});
}

// --- ToonMaterial ---
ToonMaterial::ToonMaterial(VulkanContext& context) : Material(context) { 
    InitDefaults(context); m_baseMap = s_defaultWhite; 
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_paramBuffers[i] = CreateScope<UniformBuffer>(context, sizeof(ToonParameters));
    }
}
ToonMaterial::~ToonMaterial() { 
    for (auto& buffer : m_paramBuffers) buffer.reset(); 
}
vk::DescriptorSetLayout ToonMaterial::CreateLayout(vk::Device device) {
    std::vector<vk::DescriptorSetLayoutBinding> b = {
        {0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment},
        {1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}
    };
    return device.createDescriptorSetLayout({ {}, (uint32_t)b.size(), b.data() });
}
vk::DescriptorSet ToonMaterial::getDescriptorSet(vk::DescriptorPool pool, vk::DescriptorSetLayout layout) {
    uint32_t frame = s_currentFrame;
    if (!m_sets[frame]) { 
        m_sets[frame] = m_context.getDevice().allocateDescriptorSets({ pool, 1, &layout })[0]; 
        m_dirty[frame] = true; 
    }

    if (m_baseMap && !m_baseMap->isReady()) m_dirty[frame] = true;

    if (m_dirty[frame]) { updateDescriptorSet(frame); m_dirty[frame] = false; }
    return m_sets[frame];
}
void ToonMaterial::updateDescriptorSet(uint32_t frame) {
    m_paramBuffers[frame]->update(&m_parameters, sizeof(ToonParameters));
    vk::DescriptorBufferInfo bInfo(m_paramBuffers[frame]->getHandle(), 0, sizeof(ToonParameters));
    
    Ref<Texture> tex = (m_baseMap && m_baseMap->isReady()) ? m_baseMap : s_defaultWhite;
    vk::DescriptorImageInfo iInfo(tex->getSampler(), tex->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
    
    std::vector<vk::WriteDescriptorSet> writes = {
        { m_sets[frame], 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &bInfo },
        { m_sets[frame], 1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &iInfo }
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
    uint32_t frame = s_currentFrame;
    if (!m_sets[frame]) { 
        m_sets[frame] = m_context.getDevice().allocateDescriptorSets({ pool, 1, &layout })[0]; 
        m_dirty[frame] = true; 
    }

    if (m_cubemap && !m_cubemap->isReady()) m_dirty[frame] = true;

    if (m_dirty[frame]) { updateDescriptorSet(frame); m_dirty[frame] = false; }
    return m_sets[frame];
}
void SkyboxMaterial::updateDescriptorSet(uint32_t frame) {
    if (!m_cubemap) return;
    Ref<Texture> tex = m_cubemap->isReady() ? m_cubemap : s_defaultWhite;
    vk::DescriptorImageInfo i(tex->getSampler(), tex->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
    vk::WriteDescriptorSet w(m_sets[frame], 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, &i);
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
    uint32_t frame = s_currentFrame;
    if (!m_sets[frame]) { 
        m_sets[frame] = m_context.getDevice().allocateDescriptorSets({ pool, 1, &layout })[0]; 
        m_dirty[frame] = true; 
    }

    if (m_texture && !m_texture->isReady()) m_dirty[frame] = true;

    if (m_dirty[frame]) { updateDescriptorSet(frame); m_dirty[frame] = false; }
    return m_sets[frame];
}
void SkySphereMaterial::updateDescriptorSet(uint32_t frame) {
    Ref<Texture> tex = (m_texture && m_texture->isReady()) ? m_texture : s_defaultWhite;
    vk::DescriptorImageInfo i(tex->getSampler(), tex->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
    vk::WriteDescriptorSet w(m_sets[frame], 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, &i);
    m_context.getDevice().updateDescriptorSets(1, &w, 0, nullptr);
}

} // namespace bb3d