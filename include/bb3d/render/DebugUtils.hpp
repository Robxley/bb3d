#pragma once

#include "bb3d/render/VulkanContext.hpp"
#include <array>
#include <string_view>
#include <cstdint>

#if defined(BB_PROFILE)
#include <tracy/TracyVulkan.hpp>
#endif

namespace bb3d {

/**
 * @brief Predefined thematic color palette for RenderDoc / Nsight GPU markers.
 */
struct DebugColor {
    static constexpr std::array<float, 4> ShadowPass    = {0.35f, 0.35f, 0.35f, 1.0f}; // Dark Slate Gray
    static constexpr std::array<float, 4> SkyboxPass    = {0.20f, 0.50f, 0.90f, 1.0f}; // Sky Blue
    static constexpr std::array<float, 4> ScenePbrPass  = {0.20f, 0.80f, 0.40f, 1.0f}; // Emerald Green
    static constexpr std::array<float, 4> CompositePass = {0.95f, 0.55f, 0.10f, 1.0f}; // Warm Amber
    static constexpr std::array<float, 4> PickingPass   = {0.85f, 0.15f, 0.85f, 1.0f}; // Magenta / Violet
    static constexpr std::array<float, 4> Transfer      = {0.80f, 0.80f, 0.20f, 1.0f}; // Yellow / Gold
    static constexpr std::array<float, 4> Default       = {0.70f, 0.70f, 0.70f, 1.0f}; // Neutral Silver
};

/**
 * @brief C++20 RAII Scoped Debug Label for Vulkan command buffers.
 * Automatically balances beginDebugUtilsLabelEXT and endDebugUtilsLabelEXT.
 */
class ScopedDebugLabel {
public:
    ScopedDebugLabel(VulkanContext& ctx, vk::CommandBuffer cb, std::string_view name, std::array<float, 4> color = DebugColor::Default)
        : m_ctx(&ctx), m_cb(cb), m_active(ctx.isDebugUtilsSupported()) {
        if (m_active && m_cb) {
            m_ctx->cmdBeginDebugLabel(m_cb, name, color);
        }
    }

    ~ScopedDebugLabel() {
        if (m_active && m_ctx && m_cb) {
            m_ctx->cmdEndDebugLabel(m_cb);
        }
    }

    // Non-copyable
    ScopedDebugLabel(const ScopedDebugLabel&) = delete;
    ScopedDebugLabel& operator=(const ScopedDebugLabel&) = delete;

    // Movable
    ScopedDebugLabel(ScopedDebugLabel&& other) noexcept
        : m_ctx(other.m_ctx), m_cb(other.m_cb), m_active(other.m_active) {
        other.m_active = false;
        other.m_ctx = nullptr;
    }

    ScopedDebugLabel& operator=(ScopedDebugLabel&& other) noexcept {
        if (this != &other) {
            if (m_active && m_ctx && m_cb) {
                m_ctx->cmdEndDebugLabel(m_cb);
            }
            m_ctx = other.m_ctx;
            m_cb = other.m_cb;
            m_active = other.m_active;
            other.m_active = false;
            other.m_ctx = nullptr;
        }
        return *this;
    }

private:
    VulkanContext* m_ctx = nullptr;
    vk::CommandBuffer m_cb;
    bool m_active = false;
};

} // namespace bb3d

#ifndef BB3D_CONCAT_INTERNAL
#define BB3D_CONCAT_INTERNAL_IMPL(a, b) a##b
#define BB3D_CONCAT_INTERNAL(a, b) BB3D_CONCAT_INTERNAL_IMPL(a, b)
#endif

/**
 * @brief Combined GPU zone macro: provides both Vulkan DebugUtils labeling (RenderDoc/Nsight)
 * and Tracy GPU profiling (if BB_PROFILE is active).
 */
#if defined(BB_PROFILE)
    #define BB_GPU_ZONE(tracyCtx, contextRef, cb, name, color) \
        ::bb3d::ScopedDebugLabel BB3D_CONCAT_INTERNAL(_scoped_debug_label_, __LINE__)(contextRef, cb, name, color); \
        TracyVkNamedZoneC(tracyCtx, BB3D_CONCAT_INTERNAL(_tracy_gpu_zone_, __LINE__), static_cast<VkCommandBuffer>(cb), name, \
            ((static_cast<uint32_t>((color)[0] * 255.0f) << 16) | \
             (static_cast<uint32_t>((color)[1] * 255.0f) << 8)  | \
             (static_cast<uint32_t>((color)[2] * 255.0f))), true)
#else
    #define BB_GPU_ZONE(tracyCtx, contextRef, cb, name, color) \
        ::bb3d::ScopedDebugLabel BB3D_CONCAT_INTERNAL(_scoped_debug_label_, __LINE__)(contextRef, cb, name, color)
#endif
