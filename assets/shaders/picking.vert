#version 450

// Minimal vertex shader for picking pass — only position needed
layout(location = 0) in vec3 inPosition;

// Push constant with entity ID
layout(push_constant) uniform PushConstants {
    uint entityID;
} pc;

// SSBO for instancing (same as main pass)
layout(set = 0, binding = 1) readonly buffer InstanceBuffer {
    mat4 models[];
} instances;

// Global UBO (same as main pass, we only use view/proj)
layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    // ... rest is unused
} ubo;

void main() {
    mat4 model = instances.models[gl_InstanceIndex];
    gl_Position = ubo.proj * ubo.view * model * vec4(inPosition, 1.0);
}


