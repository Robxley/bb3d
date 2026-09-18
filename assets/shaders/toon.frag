#version 450

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform MaterialUBO {
    vec4 baseColor;
    vec3 emissiveFactor;
    float alphaCutoff;
    int alphaMode;
    int doubleSided;
} mat;

layout(set = 1, binding = 1) uniform sampler2D texSampler;

struct Light {
    vec4 position;  // xyz = pos, w = type (0=Dir, 1=Point)
    vec4 color;     // rgb = color, a = intensity
    vec4 direction; // xyz = dir
    vec4 params;    // x = range
};

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    mat4 shadowCascades[4];
    vec4 shadowSplitDepths;
    vec4 camPos;
    vec4 globalParams; // .x = numLights
    vec4 ambientColor;
    vec4 shadowBiases;
    vec4 fogColor;
    vec4 fogParams;
    mat4 skyProj;
    Light lights[10];
} ubo;

layout(set = 0, binding = 2) uniform sampler2DArrayShadow shadowMap;

float ShadowCalculation(vec3 fragPosWorldSpace, vec3 N, vec3 lightDir) {
    vec4 viewPos = ubo.view * vec4(fragPosWorldSpace, 1.0);
    float depth = abs(viewPos.z);
    
    int layer = -1;
    for(int i = 0; i < 4; ++i) {
        if(depth <= ubo.shadowSplitDepths[i]) {
            layer = i;
            break;
        }
    }
    if (layer == -1) return 1.0; // Beyond shadow cascades: fully lit

    vec4 fragPosLightSpace = ubo.shadowCascades[layer] * vec4(fragPosWorldSpace, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    
    if(projCoords.z > 1.0 || projCoords.z < 0.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        return 1.0; 

    // Normal-adaptive bias
    float bias = max(0.005 * (1.0 - dot(N, lightDir)), 0.001);
    
    // PCF 3x3
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0).xy;
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            shadow += texture(shadowMap, vec4(projCoords.xy + vec2(x,y)*texelSize, layer, projCoords.z - bias));
        }
    }
    return shadow / 9.0;
}

void main() {
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(ubo.camPos.xyz - fragPos);
    
    // --- 1. Outline edge detection ---
    // Angle between normal and view direction; near 0 is silhouette edge.
    float NdotV = dot(N, V);
    
    // Outline threshold with smoothstep to reduce harsh aliasing
    float outlineThickness = 0.3; 
    float outline = smoothstep(outlineThickness, outlineThickness + 0.05, NdotV);
    
    // Black outline on edges
    if (NdotV < outlineThickness) {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // --- 2. Cel Shading illumination ---
    vec3 finalColor = vec3(0.0);
    vec3 fColor = (length(fragColor) < 0.01) ? vec3(1.0) : fragColor;
    vec3 mColor = (length(mat.baseColor.rgb) < 0.01) ? vec3(1.0) : mat.baseColor.rgb;
    vec3 albedo = texture(texSampler, fragUV).rgb * fColor * mColor;

    // Minimum ambient light
    finalColor += albedo * 0.2;

    int numLights = int(ubo.globalParams.x);
    for(int i = 0; i < numLights; ++i) {
        vec3 L;
        float attenuation = 1.0;
        
        if (ubo.lights[i].position.w < 0.5) { // Directional
            L = normalize(-ubo.lights[i].direction.xyz);
        } else { // Point
            vec3 lightDir = ubo.lights[i].position.xyz - fragPos;
            float distance = length(lightDir);
            L = normalize(lightDir);
            
            if (distance > ubo.lights[i].params.x) attenuation = 0.0;
            else attenuation = 1.0 - (distance / ubo.lights[i].params.x);
            attenuation = max(attenuation, 0.0);
        }

        float NdotL = max(dot(N, L), 0.0);
        float intensity = NdotL * attenuation;

        // Shadow on primary directional light
        float shadow = 1.0;
        if (i == 0 && ubo.lights[i].position.w < 0.5) {
            shadow = ShadowCalculation(fragPos, N, L);
        }

        // Quantization (Toon color bands)
        float stepIntensity = 0.0;
        if (intensity * shadow > 0.8) stepIntensity = 1.0;
        else if (intensity * shadow > 0.5) stepIntensity = 0.7;
        else if (intensity * shadow > 0.25) stepIntensity = 0.4;
        else stepIntensity = 0.1;

        finalColor += albedo * ubo.lights[i].color.rgb * stepIntensity;
    }

    outColor = vec4(finalColor, 1.0);
}
