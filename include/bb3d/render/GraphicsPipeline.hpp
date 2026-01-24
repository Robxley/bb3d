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
                     const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts = {},
                     bool useVertexInput = true);
    ~GraphicsPipeline();

    // Empêcher la copie
    GraphicsPipeline(const GraphicsPipeline&) = delete;
    GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;

    void bind(VkCommandBuffer commandBuffer);

    [[nodiscard]] VkPipeline getHandle() const { return m_pipeline; }
    [[nodiscard]] VkPipelineLayout getLayout() const { return m_pipelineLayout; }

private:
    void createPipelineLayout(const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts);
    void createPipeline(const Shader& vertShader, const Shader& fragShader, const EngineConfig& config, bool useVertexInput);

    VulkanContext& m_context;
    SwapChain& m_swapChain;
    VkPipelineLayout m_pipelineLayout{VK_NULL_HANDLE};
    VkPipeline m_pipeline{VK_NULL_HANDLE};
};

}