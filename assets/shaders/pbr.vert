#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inUV;
layout(location = 4) in vec4 inTangent; // Tangent + handedness (w)

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUV;
layout(location = 3) out mat3 TBN;

layout(push_constant) uniform PushConstants {
    mat4 model;
} push;

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    vec3 camPos;
} ubo;

void main() {
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * worldPos;
    fragPos = worldPos.xyz;
    fragUV = inUV;

    // Calcul TBN
    vec3 T = normalize(vec3(push.model * vec4(inTangent.xyz, 0.0)));
    vec3 N = normalize(vec3(push.model * vec4(inNormal, 0.0)));
    T = normalize(T - dot(T, N) * N); // Re-orthogonalize
    vec3 B = cross(N, T) * inTangent.w;
    TBN = mat3(T, B, N);
    
    fragNormal = N; // Fallback normal
}
