#version 450

layout(binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
} global;

layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = global.proj * global.view * pc.model * vec4(inPosition, 1.0);
    fragColor = inColor;
}