#version 450

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 fragUV;

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    mat4 shadowCascades[4];
    vec4 shadowSplitDepths;
    vec4 camPos;
    vec4 globalParams;
    vec4 ambientColor;
    vec4 shadowBiases;
    vec4 fogColor;
    vec4 fogParams;
    mat4 skyProj;
} ubo;

void main() {
    fragUV = inPosition;
    mat4 viewNoTransform = mat4(mat3(ubo.view));
    vec4 pos = ubo.skyProj * viewNoTransform * vec4(inPosition, 1.0);
    gl_Position = pos.xyww;
}

