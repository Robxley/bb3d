#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inUV;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUV;
layout(location = 3) out vec3 fragColor;

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

layout(std430, set = 0, binding = 1) readonly buffer InstanceBuffer {
    mat4 models[];
} instances;

void main() {
    mat4 modelMatrix = instances.models[gl_InstanceIndex];
    
    // Extract scale from the first column of the model matrix
    float scale = length(vec3(modelMatrix[0][0], modelMatrix[0][1], modelMatrix[0][2]));
    
    // Extract particle center position (Column 3)
    vec3 centerPos = vec3(modelMatrix[3][0], modelMatrix[3][1], modelMatrix[3][2]);
    
    // Transform center to view space
    vec4 viewPos = ubo.view * vec4(centerPos, 1.0);
    
    // Displace vertex in view space (billboarding)
    // We add the local XY coordinates (scaled) directly to the view-space position
    viewPos.xy += inPosition.xy * scale;
    
    fragPos = centerPos; 
    fragNormal = vec3(0, 0, 1); // Face to camera in view space
    fragUV = inUV;
    fragColor = inColor;

    gl_Position = ubo.proj * viewPos;
}
