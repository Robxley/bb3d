import os

# fix Components.hpp
with open('include/bb3d/scene/Components.hpp', 'r', encoding='utf-8') as f:
    text = f.read()
if 'bool castShadows = true;' not in text:
    text = text.replace('bool visible = true;', 'bool visible = true;\n    bool castShadows = true;')
with open('include/bb3d/scene/Components.hpp', 'w', encoding='utf-8') as f:
    f.write(text)

# fix Renderer.cpp
with open('src/bb3d/render/Renderer.cpp', 'r', encoding='utf-8') as f:
    text = f.read()
if '#include "bb3d/render/ShadowCascade.hpp"' not in text:
    text = text.replace('#include "bb3d/render/Renderer.hpp"', '#include "bb3d/render/Renderer.hpp"\n#include "bb3d/render/ShadowCascade.hpp"')
with open('src/bb3d/render/Renderer.cpp', 'w', encoding='utf-8') as f:
    f.write(text)

# fix Renderer.hpp properly
with open('include/bb3d/render/Renderer.hpp', 'r', encoding='utf-8') as f:
    text = f.read()

# 1. remove getShadowPass
text = text.replace('[[nodiscard]] vk::RenderPass getShadowPass() const { return m_shadowRenderPass; }\n', '')

# 2. replace shadow members
old_shadow_members = """    vk::RenderPass m_shadowRenderPass;
    vk::Image m_shadowDepthImage;
    VmaAllocation m_shadowDepthAllocation = nullptr;
    vk::ImageView m_shadowDepthView;
    vk::Sampler m_shadowSampler;"""

new_shadow_members = """    vk::Image m_shadowDepthImage;
    VmaAllocation m_shadowDepthAllocation = nullptr;
    vk::ImageView m_shadowDepthView;
    std::vector<vk::ImageView> m_shadowCascadeViews;
    vk::Sampler m_shadowSampler;"""
text = text.replace(old_shadow_members, new_shadow_members)

# 3. update GlobalUBO
old_ubo = """    struct GlobalUBO {
        glm::mat4 view;
        glm::mat4 proj;
        glm::vec4 camPos;      // .xyz = pos, .w = padding
        glm::vec4 globalParams; // .x = numLights (cast to int), .yzw = padding
        ShaderLight lights[10];
    };"""

new_ubo = """    struct GlobalUBO {
        glm::mat4 view;
        glm::mat4 proj;
        glm::mat4 shadowCascades[4];
        glm::vec4 shadowSplitDepths;
        glm::vec4 camPos;      // .xyz = pos, .w = padding
        glm::vec4 globalParams; // .x = numLights (cast to int), .yzw = padding
        ShaderLight lights[10];
    };"""
text = text.replace(old_ubo, new_ubo)

# 4. add renderShadows declaration
if 'void renderShadows' not in text:
    text = text.replace('void compositeToSwapchain(vk::CommandBuffer cb, uint32_t imageIndex);',
                        'void compositeToSwapchain(vk::CommandBuffer cb, uint32_t imageIndex);\n    void renderShadows(vk::CommandBuffer cb, Scene& scene, GlobalUBO& uboData);')

with open('include/bb3d/render/Renderer.hpp', 'w', encoding='utf-8') as f:
    f.write(text)
