#version 450

layout(binding = 0, set = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

layout(std430, binding = 0, set = 1) readonly buffer ObjectMatrices {
    mat4 model[];
} matrices;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;

void main() {
    mat4 modelMatrix = matrices.model[gl_InstanceIndex];
    gl_Position = ubo.proj * ubo.view * modelMatrix * vec4(inPosition, 1.0);
    fragColor = inColor;
}


