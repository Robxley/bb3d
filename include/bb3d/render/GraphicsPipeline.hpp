#pragma once
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/render/Shader.hpp"
#include "bb3d/render/SwapChain.hpp"
#include "bb3d/core/Config.hpp"

namespace bb3d {

class GraphicsPipeline {
public:
    // Constructor using SwapChain (legacy/direct)
    GraphicsPipeline(VulkanContext& context, SwapChain& swapChain, 
                     const Shader& vertShader, const Shader& fragShader,
                     const EngineConfig& config,
                     const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts = {},
                     const std::vector<vk::PushConstantRange>& pushConstantRanges = {},
                     bool useVertexInput = true,
                     bool depthWrite = true,
                     vk::CompareOp depthCompareOp = vk::CompareOp::eLess,
                     const std::vector<uint32_t>& enabledAttributes = {});

    // Constructor using explicit formats (offscreen)
    GraphicsPipeline(VulkanContext& context, vk::Format colorFormat, vk::Format depthFormat,
                     const Shader& vertShader, const Shader& fragShader,
                     const EngineConfig& config,
                     const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts = {},
                     const std::vector<vk::PushConstantRange>& pushConstantRanges = {},
                     bool useVertexInput = true,
                     bool depthWrite = true,
                     vk::CompareOp depthCompareOp = vk::CompareOp::eLess,
                     const std::vector<uint32_t>& enabledAttributes = {});

    ~GraphicsPipeline();

    GraphicsPipeline(const GraphicsPipeline&) = delete;
    GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;

    void bind(vk::CommandBuffer commandBuffer);

    [[nodiscard]] inline vk::Pipeline getHandle() const { return m_pipeline; }
    [[nodiscard]] inline vk::PipelineLayout getLayout() const { return m_pipelineLayout; }

private:
    void createPipelineLayout(const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts, 
                               const std::vector<vk::PushConstantRange>& pushConstantRanges);
    // Modified to take formats directly
    void createPipeline(const Shader& vertShader, const Shader& fragShader, const EngineConfig& config, 
                        bool useVertexInput, bool depthWrite, vk::CompareOp depthCompareOp, 
                        const std::vector<uint32_t>& enabledAttributes);

    VulkanContext& m_context;
    
    // Formats stored locally instead of referencing SwapChain
    vk::Format m_colorFormat;
    vk::Format m_depthFormat;

    vk::PipelineLayout m_pipelineLayout;
    vk::Pipeline m_pipeline;
};

} // namespace bb3d