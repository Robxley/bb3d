#pragma once
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/resource/Resource.hpp"
#include <string_view>
#include <vector>

namespace bb3d {

/**
 * @brief Représente un module de shader Vulkan (.spv).
 * 
 * Cette ressource encapsule un `vk::ShaderModule`.
 * Elle charge des fichiers binaire SPIR-V depuis le disque.
 */
class Shader : public Resource {
public:
    /** 
     * @brief Charge et compile un module SPIR-V.
     * @param context Contexte Vulkan.
     * @param filepath Chemin vers le fichier .spv.
     */
    Shader(VulkanContext& context, std::string_view filepath);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    /** @brief Récupère le handle Vulkan du module. */
    [[nodiscard]] inline vk::ShaderModule getModule() const { return m_module; }

private:
    std::vector<char> readFile(std::string_view filename);

    VulkanContext& m_context;
    vk::ShaderModule m_module;
};

} // namespace bb3d