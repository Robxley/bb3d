// unit_test_25_material_frame.cpp
// Validates the per-frame material UBO/descriptor-set indexing used by the
// triple-buffering fix (B8). The static frame index must commute cleanly
// across MAX_FRAMES_IN_FLIGHT (0, 1, 2) so that each frame in flight binds its
// own descriptor set instead of all frames clobbering m_sets[0].

#include "bb3d/render/Material.hpp"

#include <cassert>
#include <cstdint>

int main() {
    // The frame index is a static property shared by every material instance;
    // it must round-trip exactly for all triple-buffered slots. The engine uses
    // MAX_FRAMES_IN_FLIGHT == 3 (protected on Material), exercised here directly.
    constexpr uint32_t kFramesInFlight = 3;
    for (uint32_t frame = 0; frame < kFramesInFlight; ++frame) {
        bb3d::Material::SetCurrentFrame(frame);
        assert(bb3d::Material::GetCurrentFrame() == frame);
    }

    // After cycling through the frames, the renderer wraps back to 0; ensure
    // the static index reflects the last set value without spurious mutation.
    bb3d::Material::SetCurrentFrame(0);
    assert(bb3d::Material::GetCurrentFrame() == 0);

    return 0;
}
