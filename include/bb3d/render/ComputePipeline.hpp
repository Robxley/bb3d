#pragma once
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/render/Shader.hpp"
#include "bb3d/core/Config.hpp"

namespace bb3d {

/**
 * @brief Gère un pipeline de calcul (Compute Pipeline).
 */
class ComputePipeline {
public:
    ComputePipeline(VulkanContext& context, const Shader& computeShader,
                    const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts = {},
                    const std::vector<vk::PushConstantRange>& pushConstantRanges = {});
    ~ComputePipeline();

    void bind(vk::CommandBuffer commandBuffer);
    void dispatch(vk::CommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY = 1, uint32_t groupCountZ = 1);

    [[nodiscard]] inline vk::PipelineLayout getLayout() const { return m_pipelineLayout; }

private:
    VulkanContext& m_context;
    vk::PipelineLayout m_pipelineLayout;
    vk::Pipeline m_pipeline;
};

} // namespace bb3d
