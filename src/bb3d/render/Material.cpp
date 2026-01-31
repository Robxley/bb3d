#include "bb3d/render/Material.hpp"
#include "bb3d/core/Log.hpp"

namespace bb3d {

Ref<Texture> Material::s_defaultWhite = nullptr;
Ref<Texture> Material::s_defaultBlack = nullptr;
Ref<Texture> Material::s_defaultNormal = nullptr;

Material::Material(VulkanContext& context) : m_context(context) {
    // Initialisation des textures par défaut si nécessaire
    if (!s_defaultWhite) {
        uint32_t white = 0xFFFFFFFF;
        s_defaultWhite = CreateRef<Texture>(context, std::span<const std::byte>((const std::byte*)&white, 4), 1, 1);
    }
    if (!s_defaultBlack) {
        uint32_t black = 0xFF000000; // Alpha 255, RGB 0
        s_defaultBlack = CreateRef<Texture>(context, std::span<const std::byte>((const std::byte*)&black, 4), 1, 1);
    }
    if (!s_defaultNormal) {
        uint32_t normal = 0xFFFF8080; // RGBA: 128, 128, 255, 255 (Approx flat normal)
        s_defaultNormal = CreateRef<Texture>(context, std::span<const std::byte>((const std::byte*)&normal, 4), 1, 1);
    }

    m_albedoMap = s_defaultWhite;
    m_normalMap = s_defaultNormal;
    m_metallicMap = s_defaultBlack; // Pas de métal par défaut
    m_roughnessMap = s_defaultWhite; // Rugueux par défaut
    m_aoMap = s_defaultWhite;
    m_emissiveMap = s_defaultBlack;
}

void Material::Cleanup() {
    s_defaultWhite.reset();
    s_defaultBlack.reset();
    s_defaultNormal.reset();
}

Material::~Material() {
    // Le DescriptorSet est libéré par le pool, pas besoin de le free individuellement si le pool est reset
}

vk::DescriptorSet Material::getDescriptorSet(vk::DescriptorSetLayout layout, vk::DescriptorPool pool) {
    if (m_descriptorSet && !m_dirty) return m_descriptorSet;

    if (!m_descriptorSet) {
        vk::DescriptorSetAllocateInfo allocInfo(pool, 1, &layout);
        m_descriptorSet = m_context.getDevice().allocateDescriptorSets(allocInfo)[0];
    }

    updateDescriptorSet(m_descriptorSet);
    m_dirty = false;
    return m_descriptorSet;
}

void Material::updateDescriptorSet(vk::DescriptorSet set) {
    auto device = m_context.getDevice();

    std::vector<vk::WriteDescriptorSet> writes;

    // Helper lambda
    auto addWrite = [&](uint32_t binding, Ref<Texture> tex) {
        vk::DescriptorImageInfo* imageInfo = new vk::DescriptorImageInfo(
            tex ? tex->getSampler() : s_defaultWhite->getSampler(),
            tex ? tex->getImageView() : s_defaultWhite->getImageView(),
            vk::ImageLayout::eShaderReadOnlyOptimal
        );
        // Note: Memory leak here on imageInfo pointer for simplicity, usually stored in member or struct
        // In production, use a vector on stack or member.
        
        writes.push_back({
            set, binding, 0, 1, vk::DescriptorType::eCombinedImageSampler, imageInfo, nullptr, nullptr
        });
    };
    
    // Binding 0: Albedo
    vk::DescriptorImageInfo albedoInfo(m_albedoMap->getSampler(), m_albedoMap->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
    writes.push_back({set, 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, &albedoInfo, nullptr});

    // Binding 1: Normal
    vk::DescriptorImageInfo normalInfo(m_normalMap->getSampler(), m_normalMap->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
    writes.push_back({set, 1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &normalInfo, nullptr});

    // Binding 2: Metallic
    vk::DescriptorImageInfo metalInfo(m_metallicMap->getSampler(), m_metallicMap->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
    writes.push_back({set, 2, 0, 1, vk::DescriptorType::eCombinedImageSampler, &metalInfo, nullptr});

    // Binding 3: Roughness
    vk::DescriptorImageInfo roughInfo(m_roughnessMap->getSampler(), m_roughnessMap->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
    writes.push_back({set, 3, 0, 1, vk::DescriptorType::eCombinedImageSampler, &roughInfo, nullptr});

    // Binding 4: AO
    vk::DescriptorImageInfo aoInfo(m_aoMap->getSampler(), m_aoMap->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
    writes.push_back({set, 4, 0, 1, vk::DescriptorType::eCombinedImageSampler, &aoInfo, nullptr});

    // Binding 5: Emissive
    vk::DescriptorImageInfo emInfo(m_emissiveMap->getSampler(), m_emissiveMap->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
    writes.push_back({set, 5, 0, 1, vk::DescriptorType::eCombinedImageSampler, &emInfo, nullptr});

    device.updateDescriptorSets(writes, nullptr);
}

} // namespace bb3d