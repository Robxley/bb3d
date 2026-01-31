#include "bb3d/render/Material.hpp"
#include "bb3d/core/Log.hpp"

#include "bb3d/render/Material.hpp"

#include "bb3d/core/Log.hpp"



namespace bb3d {



Ref<Texture> Material::s_defaultWhite = nullptr;

Ref<Texture> Material::s_defaultBlack = nullptr;

Ref<Texture> Material::s_defaultNormal = nullptr;



void Material::InitDefaults(VulkanContext& context) {

    if (!s_defaultWhite) {

        uint32_t white = 0xFFFFFFFF;

        s_defaultWhite = CreateRef<Texture>(context, std::span<const std::byte>((const std::byte*)&white, 4), 1, 1);

    }

    if (!s_defaultBlack) {

        uint32_t black = 0xFF000000;

        s_defaultBlack = CreateRef<Texture>(context, std::span<const std::byte>((const std::byte*)&black, 4), 1, 1);

    }

    if (!s_defaultNormal) {

        uint32_t normal = 0xFFFF8080;

        s_defaultNormal = CreateRef<Texture>(context, std::span<const std::byte>((const std::byte*)&normal, 4), 1, 1);

    }

}



void Material::Cleanup() {

    s_defaultWhite.reset();

    s_defaultBlack.reset();

    s_defaultNormal.reset();

}



// --- PBRMaterial ---



PBRMaterial::PBRMaterial(VulkanContext& context) : Material(context) {

    InitDefaults(context);

    m_albedoMap = s_defaultWhite;

    m_normalMap = s_defaultNormal;

    m_metallicMap = s_defaultBlack;

    m_roughnessMap = s_defaultWhite;

    m_aoMap = s_defaultWhite;

    m_emissiveMap = s_defaultBlack;

}



vk::DescriptorSetLayout PBRMaterial::getDescriptorSetLayout(vk::Device device) {

    if (m_layout) return m_layout;



    std::vector<vk::DescriptorSetLayoutBinding> bindings = {

        {0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}, // Albedo

        {1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}, // Normal

        {2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}, // Metallic

        {3, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}, // Roughness

        {4, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}, // AO

        {5, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}, // Emissive

    };

    

    m_layout = device.createDescriptorSetLayout({ {}, (uint32_t)bindings.size(), bindings.data() });

    return m_layout;

}



vk::DescriptorSet PBRMaterial::getDescriptorSet(vk::DescriptorPool pool) {

    if (m_set && !m_dirty) return m_set;



    if (!m_set) {

        auto layout = getDescriptorSetLayout(m_context.getDevice());

        vk::DescriptorSetAllocateInfo allocInfo(pool, 1, &layout);

        m_set = m_context.getDevice().allocateDescriptorSets(allocInfo)[0];

    }



    updateDescriptorSet();

    m_dirty = false;

    return m_set;

}



void PBRMaterial::updateDescriptorSet() {

    auto device = m_context.getDevice();

    std::vector<vk::WriteDescriptorSet> writes;



    auto addWrite = [&](uint32_t binding, Ref<Texture> tex) {

        vk::DescriptorImageInfo imageInfo(

            tex ? tex->getSampler() : s_defaultWhite->getSampler(),

            tex ? tex->getImageView() : s_defaultWhite->getImageView(),

            vk::ImageLayout::eShaderReadOnlyOptimal

        );

        // Note: vector push_back invalidates pointers if realloc, so we construct Writes carefully.

        // Here we use a temp vector of WriteDescriptorSet, but pImageInfo must point to valid memory.

        // This implementation is unsafe if imageInfo goes out of scope before updateDescriptorSets.

        // FIX: Update one by one or keep infos alive.

        

        vk::WriteDescriptorSet w(m_set, binding, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imageInfo, nullptr, nullptr);

        device.updateDescriptorSets(1, &w, 0, nullptr);

    };



    addWrite(0, m_albedoMap);

    addWrite(1, m_normalMap);

    addWrite(2, m_metallicMap);

    addWrite(3, m_roughnessMap);

    addWrite(4, m_aoMap);

    addWrite(5, m_emissiveMap);

}



// --- UnlitMaterial ---



UnlitMaterial::UnlitMaterial(VulkanContext& context) : Material(context) {

    InitDefaults(context);

    m_baseMap = s_defaultWhite;

}



vk::DescriptorSetLayout UnlitMaterial::getDescriptorSetLayout(vk::Device device) {

    if (m_layout) return m_layout;

    vk::DescriptorSetLayoutBinding binding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment);

    m_layout = device.createDescriptorSetLayout({ {}, 1, &binding });

    return m_layout;

}



vk::DescriptorSet UnlitMaterial::getDescriptorSet(vk::DescriptorPool pool) {

    if (m_set && !m_dirty) return m_set;

    if (!m_set) {

        auto layout = getDescriptorSetLayout(m_context.getDevice());

        vk::DescriptorSetAllocateInfo allocInfo(pool, 1, &layout);

        m_set = m_context.getDevice().allocateDescriptorSets(allocInfo)[0];

    }

    updateDescriptorSet();

    m_dirty = false;

    return m_set;

}



void UnlitMaterial::updateDescriptorSet() {

    vk::DescriptorImageInfo imageInfo(m_baseMap->getSampler(), m_baseMap->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

    vk::WriteDescriptorSet write(m_set, 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imageInfo, nullptr, nullptr);

    m_context.getDevice().updateDescriptorSets(1, &write, 0, nullptr);

}



// --- ToonMaterial ---



ToonMaterial::ToonMaterial(VulkanContext& context) : Material(context) {

    InitDefaults(context);

    m_baseMap = s_defaultWhite;

}



vk::DescriptorSetLayout ToonMaterial::getDescriptorSetLayout(vk::Device device) {

    if (m_layout) return m_layout;

    vk::DescriptorSetLayoutBinding binding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment);

    m_layout = device.createDescriptorSetLayout({ {}, 1, &binding });

    return m_layout;

}



vk::DescriptorSet ToonMaterial::getDescriptorSet(vk::DescriptorPool pool) {

    if (m_set && !m_dirty) return m_set;

    if (!m_set) {

        auto layout = getDescriptorSetLayout(m_context.getDevice());

        vk::DescriptorSetAllocateInfo allocInfo(pool, 1, &layout);

        m_set = m_context.getDevice().allocateDescriptorSets(allocInfo)[0];

    }

    updateDescriptorSet();

    m_dirty = false;

    return m_set;

}



void ToonMaterial::updateDescriptorSet() {

    vk::DescriptorImageInfo imageInfo(m_baseMap->getSampler(), m_baseMap->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

    vk::WriteDescriptorSet write(m_set, 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imageInfo, nullptr, nullptr);

    m_context.getDevice().updateDescriptorSets(1, &write, 0, nullptr);

}



} // namespace bb3d