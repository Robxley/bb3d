#pragma once
#include "bb3d/render/Buffer.hpp"

namespace bb3d {

class UniformBuffer : public Buffer {
public:
    UniformBuffer(VulkanContext& context, VkDeviceSize size);
    ~UniformBuffer() = default;

    // Met à jour les données du buffer (le buffer est mappé en permanence car CPU_TO_GPU)
    void update(const void* data, VkDeviceSize size);
};

} // namespace bb3d
