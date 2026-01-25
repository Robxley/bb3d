#pragma once
#include "bb3d/render/Buffer.hpp"

namespace bb3d {

class UniformBuffer : public Buffer {
public:
    UniformBuffer(VulkanContext& context, vk::DeviceSize size);
    ~UniformBuffer() = default;

    inline void update(const void* data, vk::DeviceSize size) {
        upload(data, size);
    }
};

} // namespace bb3d