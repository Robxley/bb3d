#include "bb3d/render/GraphicsPipeline.hpp"
#include "bb3d/render/Vertex.hpp"
#include "bb3d/core/Log.hpp"
#include <vector>

namespace bb3d {

GraphicsPipeline::GraphicsPipeline(VulkanContext& context, SwapChain& swapChain, 
                                   const Shader& vertShader, const Shader& fragShader,
                                   const EngineConfig& config,
                                   const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts,
                                   bool useVertexInput)
    : m_context(context), m_swapChain(swapChain) {
    
    createPipelineLayout(descriptorSetLayouts);
    createPipeline(vertShader, fragShader, config, useVertexInput);
}

GraphicsPipeline::~GraphicsPipeline() {
    auto device = m_context.getDevice();
    if (m_pipeline) device.destroyPipeline(m_pipeline);
    if (m_pipelineLayout) device.destroyPipelineLayout(m_pipelineLayout);
}

void GraphicsPipeline::bind(vk::CommandBuffer commandBuffer) {
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline);
}

void GraphicsPipeline::createPipelineLayout(const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts) {
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo({}, static_cast<uint32_t>(descriptorSetLayouts.size()), descriptorSetLayouts.data());
    m_pipelineLayout = m_context.getDevice().createPipelineLayout(pipelineLayoutInfo);
}

void GraphicsPipeline::createPipeline(const Shader& vertShader, const Shader& fragShader, const EngineConfig& config, bool useVertexInput) {
    // 1. Shader Stages
    std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages = {
        vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, vertShader.getModule(), "main"),
        vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eFragment, fragShader.getModule(), "main")
    };

    // 2. Vertex Input
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    if (useVertexInput) {
        vertexInputInfo.setVertexBindingDescriptions(bindingDescription);
        vertexInputInfo.setVertexAttributeDescriptions(attributeDescriptions);
    }

    // 3. Input Assembly
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly({}, vk::PrimitiveTopology::eTriangleList, VK_FALSE);

    // 4. Viewport & Scissor (Dynamic)
    vk::PipelineViewportStateCreateInfo viewportState({}, 1, nullptr, 1, nullptr);

    std::array<vk::DynamicState, 2> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
    vk::PipelineDynamicStateCreateInfo dynamicState({}, static_cast<uint32_t>(dynamicStates.size()), dynamicStates.data());

    // 5. Rasterizer
    vk::PolygonMode polyMode = vk::PolygonMode::eFill;
    if (config.rasterizer.polygonMode == "Line") polyMode = vk::PolygonMode::eLine;
    else if (config.rasterizer.polygonMode == "Point") polyMode = vk::PolygonMode::ePoint;

    vk::CullModeFlags cullMode = vk::CullModeFlagBits::eBack;
    if (config.rasterizer.cullMode == "None") cullMode = vk::CullModeFlagBits::eNone;
    else if (config.rasterizer.cullMode == "Front") cullMode = vk::CullModeFlagBits::eFront;
    else if (config.rasterizer.cullMode == "FrontAndBack") cullMode = vk::CullModeFlagBits::eFrontAndBack;

    vk::FrontFace frontFace = (config.rasterizer.frontFace == "CW") ? vk::FrontFace::eClockwise : vk::FrontFace::eCounterClockwise;

    vk::PipelineRasterizationStateCreateInfo rasterizer({}, VK_FALSE, VK_FALSE, polyMode, cullMode, frontFace, VK_FALSE, 0.0f, 0.0f, 0.0f, 1.0f);

    // 6. Multisampling
    vk::PipelineMultisampleStateCreateInfo multisampling({}, vk::SampleCountFlagBits::e1, VK_FALSE);

    // 7. Depth Stencil
    vk::CompareOp depthOp = vk::CompareOp::eLess;
    if (config.depthStencil.depthCompareOp == "LessOrEqual") depthOp = vk::CompareOp::eLessOrEqual;
    // ... handle other ops if needed

    vk::PipelineDepthStencilStateCreateInfo depthStencil({}, 
        config.depthStencil.depthTest ? VK_TRUE : VK_FALSE,
        config.depthStencil.depthWrite ? VK_TRUE : VK_FALSE,
        depthOp, VK_FALSE, config.depthStencil.stencilTest ? VK_TRUE : VK_FALSE);

    // 8. Color Blending
    vk::PipelineColorBlendAttachmentState colorBlendAttachment(VK_FALSE);
    colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    
    vk::PipelineColorBlendStateCreateInfo colorBlending({}, VK_FALSE, vk::LogicOp::eCopy, 1, &colorBlendAttachment);

    // 9. Dynamic Rendering Info
    vk::Format colorFormat = m_swapChain.getImageFormat();
    vk::Format depthFormat = m_swapChain.getDepthFormat();

    vk::PipelineRenderingCreateInfo renderingInfo(0, 1, &colorFormat, depthFormat);

    // 10. Create Pipeline
    vk::GraphicsPipelineCreateInfo pipelineInfo({}, static_cast<uint32_t>(shaderStages.size()), shaderStages.data(), &vertexInputInfo, &inputAssembly, nullptr, &viewportState, &rasterizer, &multisampling, &depthStencil, &colorBlending, &dynamicState, m_pipelineLayout);
    pipelineInfo.pNext = &renderingInfo;

    auto result = m_context.getDevice().createGraphicsPipeline(nullptr, pipelineInfo);
    if (result.result != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to create graphics pipeline!");
    }
    m_pipeline = result.value;
}

} // namespace bb3d
