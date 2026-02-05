#include "bb3d/render/ComputePipeline.hpp"
#include "bb3d/core/Log.hpp"

namespace bb3d {

ComputePipeline::ComputePipeline(VulkanContext& context, const Shader& computeShader,
                                const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts,
                                const std::vector<vk::PushConstantRange>& pushConstantRanges)
    : m_context(context) {
    
    vk::PipelineLayoutCreateInfo layoutInfo({}, 
        static_cast<uint32_t>(descriptorSetLayouts.size()), descriptorSetLayouts.data(),
        static_cast<uint32_t>(pushConstantRanges.size()), pushConstantRanges.data());
    
    m_pipelineLayout = m_context.getDevice().createPipelineLayout(layoutInfo);

    vk::ComputePipelineCreateInfo pipelineInfo({}, 
        vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eCompute, computeShader.getModule(), "main"),
        m_pipelineLayout);

    auto result = m_context.getDevice().createComputePipeline(nullptr, pipelineInfo);
    if (result.result != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to create compute pipeline!");
    }
    m_pipeline = result.value;
}

ComputePipeline::~ComputePipeline() {
    auto dev = m_context.getDevice();
    if (m_pipeline) dev.destroyPipeline(m_pipeline);
    if (m_pipelineLayout) dev.destroyPipelineLayout(m_pipelineLayout);
}

void ComputePipeline::bind(vk::CommandBuffer commandBuffer) {
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline);
}

void ComputePipeline::dispatch(vk::CommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    commandBuffer.dispatch(groupCountX, groupCountY, groupCountZ);
}

} // namespace bb3d
