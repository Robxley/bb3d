#include "bb3d/render/Shader.hpp"
#include "bb3d/core/Log.hpp"
#include <fstream>
#include <filesystem>

namespace bb3d {

Shader::Shader(VulkanContext& context, std::string_view filepath)
    : m_context(context) {
    
    auto code = readFile(filepath);

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    if (vkCreateShaderModule(m_context.getDevice(), &createInfo, nullptr, &m_module) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module!");
    }
}

Shader::~Shader() {
    if (m_module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(m_context.getDevice(), m_module, nullptr);
    }
}

std::vector<char> Shader::readFile(std::string_view filename) {
    // Vérification de l'existence
    if (!std::filesystem::exists(filename)) {
        BB_CORE_ERROR("Shader file not found: {}", filename);
        throw std::runtime_error("Shader file not found");
    }

    std::ifstream file(std::string(filename), std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        BB_CORE_ERROR("Failed to open shader file: {}", filename);
        throw std::runtime_error("Failed to open file");
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

}
