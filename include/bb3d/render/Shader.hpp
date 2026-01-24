#pragma once
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/resource/Resource.hpp" // Added
#include <string_view>
#include <vector>

namespace bb3d {

class Shader : public Resource { // Inherit
public:
    Shader(VulkanContext& context, std::string_view filepath);
    ~Shader();

    // Empêcher la copie
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    [[nodiscard]] VkShaderModule getModule() const { return m_module; }

private:
    std::vector<char> readFile(std::string_view filename);

    VulkanContext& m_context;
    VkShaderModule m_module{VK_NULL_HANDLE};
};

}
