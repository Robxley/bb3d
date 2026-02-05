#include "bb3d/render/GraphicsPipeline.hpp"
#include "bb3d/render/Vertex.hpp"
#include "bb3d/core/Log.hpp"
#include <vector>

namespace bb3d {

// Constructeur via SwapChain (pour backward compatibility et usage direct)
GraphicsPipeline::GraphicsPipeline(VulkanContext& context, SwapChain& swapChain, 
                                   const Shader& vertShader, const Shader& fragShader,
                                   const EngineConfig& config,
                                   const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts,
                                   const std::vector<vk::PushConstantRange>& pushConstantRanges,
                                   bool useVertexInput,
                                   bool depthWrite,
                                   vk::CompareOp depthCompareOp,
                                   const std::vector<uint32_t>& enabledAttributes)
    : m_context(context) {
    
    m_colorFormat = swapChain.getImageFormat();
    m_depthFormat = swapChain.getDepthFormat();

    createPipelineLayout(descriptorSetLayouts, pushConstantRanges);
    createPipeline(vertShader, fragShader, config, useVertexInput, depthWrite, depthCompareOp, enabledAttributes);
}

// Constructeur via Formats Explicites (pour RenderTarget / Offscreen)
GraphicsPipeline::GraphicsPipeline(VulkanContext& context, vk::Format colorFormat, vk::Format depthFormat, 
                                   const Shader& vertShader, const Shader& fragShader,
                                   const EngineConfig& config,
                                   const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts,
                                   const std::vector<vk::PushConstantRange>& pushConstantRanges,
                                   bool useVertexInput,
                                   bool depthWrite,
                                   vk::CompareOp depthCompareOp,
                                   const std::vector<uint32_t>& enabledAttributes)
    : m_context(context), m_colorFormat(colorFormat), m_depthFormat(depthFormat) {

    createPipelineLayout(descriptorSetLayouts, pushConstantRanges);
    createPipeline(vertShader, fragShader, config, useVertexInput, depthWrite, depthCompareOp, enabledAttributes);
}


GraphicsPipeline::~GraphicsPipeline() {
    auto device = m_context.getDevice();
    if (m_pipeline) device.destroyPipeline(m_pipeline);
    if (m_pipelineLayout) device.destroyPipelineLayout(m_pipelineLayout);
}

void GraphicsPipeline::bind(vk::CommandBuffer commandBuffer) {
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline);
}

void GraphicsPipeline::createPipelineLayout(const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts,
                                             const std::vector<vk::PushConstantRange>& pushConstantRanges) {
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo({}, 
        static_cast<uint32_t>(descriptorSetLayouts.size()), descriptorSetLayouts.data(),
        static_cast<uint32_t>(pushConstantRanges.size()), pushConstantRanges.data());
    m_pipelineLayout = m_context.getDevice().createPipelineLayout(pipelineLayoutInfo);
}

void GraphicsPipeline::createPipeline(const Shader& vertShader, const Shader& fragShader, const EngineConfig& config, 
                                     bool useVertexInput, bool depthWrite, vk::CompareOp depthCompareOp, 
                                     const std::vector<uint32_t>& enabledAttributes) {
    std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages = {
        vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, vertShader.getModule(), "main"),
        vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eFragment, fragShader.getModule(), "main")
    };

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
    auto bindingDescription = Vertex::getBindingDescription();
    auto allAttributes = Vertex::getAttributeDescriptions();
    std::vector<vk::VertexInputAttributeDescription> filteredAttributes;

    if (useVertexInput) {
        if (enabledAttributes.empty()) {
            for(const auto& a : allAttributes) filteredAttributes.push_back(a);
        } else {
            for (uint32_t loc : enabledAttributes) {
                if (loc < allAttributes.size()) filteredAttributes.push_back(allAttributes[loc]);
            }
        }
        vertexInputInfo.setVertexBindingDescriptions(bindingDescription);
        vertexInputInfo.setVertexAttributeDescriptions(filteredAttributes);
    }

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly({}, vk::PrimitiveTopology::eTriangleList, VK_FALSE);
    vk::PipelineViewportStateCreateInfo viewportState({}, 1, nullptr, 1, nullptr);
    std::array<vk::DynamicState, 2> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
    vk::PipelineDynamicStateCreateInfo dynamicState({}, static_cast<uint32_t>(dynamicStates.size()), dynamicStates.data());

    vk::PolygonMode polyMode = vk::PolygonMode::eFill;
    if (config.rasterizer.polygonMode == "Line") polyMode = vk::PolygonMode::eLine;
    else if (config.rasterizer.polygonMode == "Point") polyMode = vk::PolygonMode::ePoint;

    vk::CullModeFlags cullMode = vk::CullModeFlagBits::eBack;
    if (config.rasterizer.cullMode == "None") cullMode = vk::CullModeFlagBits::eNone;
    else if (config.rasterizer.cullMode == "Front") cullMode = vk::CullModeFlagBits::eFront;
    else if (config.rasterizer.cullMode == "FrontAndBack") cullMode = vk::CullModeFlagBits::eFrontAndBack;

    vk::FrontFace frontFace = (config.rasterizer.frontFace == "CW") ? vk::FrontFace::eClockwise : vk::FrontFace::eCounterClockwise;
    vk::PipelineRasterizationStateCreateInfo rasterizer({}, VK_FALSE, VK_FALSE, polyMode, cullMode, frontFace, VK_FALSE, 0.0f, 0.0f, 0.0f, 1.0f);

    vk::PipelineMultisampleStateCreateInfo multisampling({}, vk::SampleCountFlagBits::e1, VK_FALSE);
    vk::PipelineDepthStencilStateCreateInfo depthStencil({}, 
        config.depthStencil.depthTest ? VK_TRUE : VK_FALSE,
        depthWrite ? VK_TRUE : VK_FALSE,
        depthCompareOp, VK_FALSE, config.depthStencil.stencilTest ? VK_TRUE : VK_FALSE);

    vk::PipelineColorBlendAttachmentState colorBlendAttachment(VK_FALSE);
    colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    vk::PipelineColorBlendStateCreateInfo colorBlending({}, VK_FALSE, vk::LogicOp::eCopy, 1, &colorBlendAttachment);

    // Utilisation des formats stockés
    vk::PipelineRenderingCreateInfo renderingInfo(0, 1, &m_colorFormat, m_depthFormat);

    vk::GraphicsPipelineCreateInfo pipelineInfo({}, static_cast<uint32_t>(shaderStages.size()), shaderStages.data(), &vertexInputInfo, &inputAssembly, nullptr, &viewportState, &rasterizer, &multisampling, &depthStencil, &colorBlending, &dynamicState, m_pipelineLayout);
    pipelineInfo.pNext = &renderingInfo;

    auto result = m_context.getDevice().createGraphicsPipeline(nullptr, pipelineInfo);
    if (result.result != vk::Result::eSuccess) throw std::runtime_error("Failed to create graphics pipeline!");
    m_pipeline = result.value;
}

} // namespace bb3d
