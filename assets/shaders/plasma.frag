#version 450

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    mat4 shadowCascades[4];
    vec4 shadowSplitDepths;
    vec4 camPos;
    vec4 globalParams; // .x = numLights
    vec4 ambientColor; // .rgb = color, .a = intensity
    vec4 shadowBiases;
    vec4 fogColor;
    vec4 fogParams;
    mat4 skyProj;
} ubo;

layout(set = 1, binding = 0) uniform MaterialUBO {
    vec4 baseColor;
    float time;
    float intensity;
} mat;

layout(set = 1, binding = 1) uniform sampler2D texSampler;

// Simple hash for noise
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f*f*(3.0-2.0*f);
    return mix(mix(hash(i + vec2(0.0,0.0)), hash(i + vec2(1.0,0.0)), u.x),
               mix(hash(i + vec2(0.0,1.0)), hash(i + vec2(1.0,1.0)), u.x), u.y);
}

void main() {
    // 1. Density mask from texture
    vec4 tex = texture(texSampler, fragUV);
    float mask = tex.r;
    
    if (mask < 0.01) discard;

    // 2. Procedural Noise Animation
    vec2 uv = fragUV;
    uv.y -= mat.time * 2.5; // Flowing downwards/away
    float n = noise(uv * 10.0 + vec2(0.0, mat.time * 5.0));
    
    // 3. Color Gradient
    // Core (Blue/White) -> Mid (Orange) -> Edge (Red)
    vec3 coreColor = vec3(0.5, 0.8, 1.0);
    vec3 midColor = vec3(1.0, 0.5, 0.1);
    vec3 outerColor = vec3(0.8, 0.1, 0.0);
    
    float centerDist = abs(fragUV.x - 0.5) * 2.0;
    float gradient = clamp(fragUV.y * 1.5 + centerDist * 0.5, 0.0, 1.0);
    
    vec3 plasma = mix(coreColor, midColor, gradient);
    plasma = mix(plasma, outerColor, gradient * gradient);
    
    // Add noise flickering
    plasma *= (0.8 + 0.4 * n);
    
    // 4. Final Color with Additive Blending in mind
    float alpha = mask * (1.0 - gradient * 0.5);
    outColor = vec4(plasma * mat.intensity * alpha, alpha);
}
