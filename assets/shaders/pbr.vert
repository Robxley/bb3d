#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inUV;
layout(location = 4) in vec4 inTangent;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUV;
layout(location = 3) out mat3 TBN;
layout(location = 6) out vec3 fragColor;

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    vec4 camPos;
    // Les autres champs (lights) ne sont pas nécessaires ici
} ubo;

layout(push_constant) uniform Push {
    mat4 model;
} push;

void main() {
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    fragPos = worldPos.xyz;
    fragNormal = normalize(mat3(push.model) * inNormal);
    fragUV = inUV;
    fragColor = inColor;

    vec3 T = normalize(mat3(push.model) * inTangent.xyz);
    vec3 N = fragNormal;
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T) * inTangent.w;
    TBN = mat3(T, B, N);

    gl_Position = ubo.proj * ubo.view * worldPos;
}
