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
} ubo;

// Nouveau : Buffer de stockage pour l'instancing
layout(std430, set = 0, binding = 1) readonly buffer InstanceBuffer {
    mat4 models[];
} instances;

void main() {
    mat4 modelMatrix = instances.models[gl_InstanceIndex];
    
    vec4 worldPos = modelMatrix * vec4(inPosition, 1.0);
    fragPos = worldPos.xyz;
    fragNormal = normalize(mat3(modelMatrix) * inNormal);
    fragUV = inUV;
    fragColor = inColor;

    vec3 T = normalize(mat3(modelMatrix) * inTangent.xyz);
    vec3 N = fragNormal;
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T) * inTangent.w;
    TBN = mat3(T, B, N);

    gl_Position = ubo.proj * ubo.view * worldPos;
}
