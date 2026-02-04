# Bug Report: Offscreen Rendering - Missing Planes

## Description
When enabling offscreen rendering in the demo engine, the planes (aircraft models) are not visible in the final output. The scene renders correctly when rendering directly to the swapchain, but fails when rendering to an intermediate render target and then compositing to the swapchain.

## Reproduction Steps
1. Run the demo engine with offscreen rendering enabled:
   ```cpp
   auto engine = Engine::Create(EngineConfig()
       .title("BB3D Demo - Offscreen 50%")
       .resolution(1280, 720)
       .vsync(true)
       .enableOffscreenRendering(true)
       .renderScale(0.5f)
   );
   ```

2. Observe that the planes (aircraft models) are not visible in the rendered output.

## Root Cause Analysis

After reviewing the code, I found several potential issues in the offscreen rendering implementation:

### 1. Missing Depth Buffer Transition in Composite Pass

In `Renderer::compositeToSwapchain()`, the depth buffer transition is missing. When rendering to the swapchain directly, the depth buffer is properly transitioned, but in the composite pass, only the color buffer transition is handled.

**Location**: `src/bb3d/render/Renderer.cpp:450-460`

The composite pass begins rendering without properly setting up the depth buffer state. This could cause depth testing to fail or behave incorrectly.

### 2. Incorrect Render Target Depth Format

In `RenderTarget::createImages()`, the depth image is created with the correct format, but there's no explicit transition to ensure it's in the right layout before rendering.

**Location**: `src/bb3d/render/RenderTarget.cpp:100-120`

The depth image should be transitioned to `vk::ImageLayout::eDepthStencilAttachmentOptimal` before first use.

### 3. Missing Depth Buffer Clear in Composite Pass

In `Renderer::compositeToSwapchain()`, the rendering attachment info for the composite pass doesn't include a depth buffer:

```cpp
vk::RenderingAttachmentInfo colorAttr(m_swapChain->getImageViews()[imageIndex], vk::ImageLayout::eColorAttachmentOptimal, vk::ResolveModeFlagBits::eNone, {}, vk::ImageLayout::eUndefined, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::ClearValue(vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f})));

cb.beginRendering({ {}, {{0, 0}, m_swapChain->getExtent()}, 1, 0, 1, &colorAttr, nullptr });
```

The `nullptr` for the depth attachment means no depth testing is performed during the composite pass, which could cause depth-related rendering issues.

### 4. Potential Depth Range Issues

When rendering to a render target and then compositing, the depth values might not be properly preserved or transformed. The composite shader (`copy.frag`) should ensure proper depth handling.

## Recommended Fixes

### Fix 1: Add Depth Buffer to Composite Pass

Modify the composite pass to include a depth buffer:

```cpp
// Add depth attachment for composite pass
vk::RenderingAttachmentInfo depthAttr(m_swapChain->getDepthImageView(), 
                                      vk::ImageLayout::eDepthStencilAttachmentOptimal, 
                                      vk::ResolveModeFlagBits::eNone, {}, 
                                      vk::ImageLayout::eUndefined, 
                                      vk::AttachmentLoadOp::eLoad, 
                                      vk::AttachmentStoreOp::eStore, 
                                      vk::ClearValue(vk::ClearDepthStencilValue(1.0f, 0)));

cb.beginRendering({ {}, {{0, 0}, m_swapChain->getExtent()}, 1, 0, 1, &colorAttr, &depthAttr });
```

### Fix 2: Ensure Proper Depth Transitions

Add explicit depth buffer transitions in both the main render pass and composite pass:

```cpp
// In drawScene() when using offscreen rendering:
vk::ImageMemoryBarrier depthBarrier({}, vk::AccessFlagBits::eDepthStencilAttachmentWrite, 
                                    vk::ImageLayout::eUndefined, 
                                    vk::ImageLayout::eDepthStencilAttachmentOptimal, 
                                    VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, 
                                    m_renderTarget->getDepthImage(), 
                                    { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 });
cb.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, 
                   vk::PipelineStageFlagBits::eEarlyFragmentTests, {}, 
                   nullptr, nullptr, depthBarrier);
```

### Fix 3: Verify Composite Shader

Ensure the composite shader (`assets/shaders/copy.frag`) properly handles depth values:

```glsl
// In copy.frag:
layout(location = 0) out vec4 outColor;
layout(location = 1) out float outDepth;

void main() {
    vec2 uv = ...; // Calculate UV coordinates
    vec4 color = texture(sampler2D(tex, sampler), uv);
    outColor = color;
    outDepth = ...; // Ensure proper depth output
}
```

## Additional Recommendations

1. **Debug Rendering**: Add a debug mode that renders the render target directly to a quadrant of the screen to verify its contents.

2. **Depth Visualization**: Implement a depth visualization shader to debug depth buffer contents.

3. **Validation Layers**: Ensure Vulkan validation layers are enabled to catch any incorrect image layout transitions or usage.

4. **Clear Values**: Verify that clear values for both color and depth buffers are appropriate for the scene.

## Testing

After applying these fixes, test with:
1. Different render scales (0.5, 0.75, 1.0)
2. Different scene complexities
3. Both PBR and unlit materials
4. Various camera positions and orientations

## Related Code Files

- `src/bb3d/render/Renderer.cpp` (lines 250-350, 450-500)
- `src/bb3d/render/RenderTarget.cpp` (lines 100-125)
- `assets/shaders/copy.frag`
- `tests/demo_engine.cpp` (offscreen rendering configuration)
