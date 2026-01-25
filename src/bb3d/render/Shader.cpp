#include "bb3d/render/Shader.hpp"
#include "bb3d/core/Log.hpp"
#include <fstream>

namespace bb3d {

Shader::Shader(VulkanContext& context, std::string_view filepath)
    : m_context(context) {
    auto code = readFile(filepath);

    vk::ShaderModuleCreateInfo createInfo({}, code.size(), reinterpret_cast<const uint32_t*>(code.data()));
    m_module = m_context.getDevice().createShaderModule(createInfo);
    
    BB_CORE_INFO("Shader: Module créé depuis {}", filepath);
}

Shader::~Shader() {
    if (m_module) {
        m_context.getDevice().destroyShaderModule(m_module);
        BB_CORE_TRACE("Shader: Destroyed shader module for {}", m_path);
    }
}

std::vector<char> Shader::readFile(std::string_view filename) {
    std::ifstream file(filename.data(), std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        BB_CORE_ERROR("Shader: Impossible d'ouvrir {}", filename);
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

} // namespace bb3d