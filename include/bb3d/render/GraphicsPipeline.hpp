#pragma once
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/render/Shader.hpp"
#include "bb3d/render/SwapChain.hpp"
#include "bb3d/core/Config.hpp"

namespace bb3d {

class GraphicsPipeline {
public:
    GraphicsPipeline(VulkanContext& context, SwapChain& swapChain, 
                     const Shader& vertShader, const Shader& fragShader,
                     const EngineConfig& config,
                     const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts = {},
                     const std::vector<vk::PushConstantRange>& pushConstantRanges = {},
                     bool useVertexInput = true);
    ~GraphicsPipeline();

    GraphicsPipeline(const GraphicsPipeline&) = delete;
    GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;

    void bind(vk::CommandBuffer commandBuffer);

    [[nodiscard]] inline vk::Pipeline getHandle() const { return m_pipeline; }
    [[nodiscard]] inline vk::PipelineLayout getLayout() const { return m_pipelineLayout; }

private:
    void createPipelineLayout(const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts, 
                               const std::vector<vk::PushConstantRange>& pushConstantRanges);
    void createPipeline(const Shader& vertShader, const Shader& fragShader, const EngineConfig& config, bool useVertexInput);

    VulkanContext& m_context;
    SwapChain& m_swapChain;
    vk::PipelineLayout m_pipelineLayout;
    vk::Pipeline m_pipeline;
};

} // namespace bb3d
