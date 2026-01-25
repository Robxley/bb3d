#pragma once
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/resource/Resource.hpp"
#include <string_view>
#include <vector>

namespace bb3d {

class Shader : public Resource {
public:
    Shader(VulkanContext& context, std::string_view filepath);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    [[nodiscard]] inline vk::ShaderModule getModule() const { return m_module; }

private:
    std::vector<char> readFile(std::string_view filename);

    VulkanContext& m_context;
    vk::ShaderModule m_module;
};

} // namespace bb3d