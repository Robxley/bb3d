#version 450

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in mat3 TBN;
layout(location = 6) in vec3 fragColor; 

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform MaterialUBO {
    vec4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    float normalScale;
    float occlusionStrength;
} mat;

layout(set = 1, binding = 1) uniform sampler2D albedoMap;
layout(set = 1, binding = 2) uniform sampler2D normalMap;
layout(set = 1, binding = 3) uniform sampler2D ormMap; // R=Occlusion, G=Roughness, B=Metallic
layout(set = 1, binding = 4) uniform sampler2D emissiveMap;

struct Light {
    vec4 position;  // xyz = pos, w = type (0=Dir, 1=Point)
    vec4 color;     // rgb = color, a = intensity
    vec4 direction; // xyz = dir
    vec4 params;    // x = range
};

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    vec4 camPos;
    vec4 globalParams; // .x = numLights
    Light lights[10];
} ubo;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 calculatePBR(vec3 L, vec3 V, vec3 N, vec3 radiance, vec3 albedo, float metallic, float roughness, vec3 F0) {
    vec3 H = normalize(V + L);
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    float NdotL = max(dot(N, L), 0.0);
    return (kD * albedo / PI + specular) * radiance * NdotL;
}

void main() {
    vec3 albedo = texture(albedoMap, fragUV).rgb * mat.baseColorFactor.rgb * fragColor;
    
    vec3 orm = texture(ormMap, fragUV).rgb;
    float ao = orm.r * mat.occlusionStrength;
    float roughness = orm.g * mat.roughnessFactor;
    float metallic = orm.b * mat.metallicFactor;
    
    vec3 emissive = texture(emissiveMap, fragUV).rgb;

    vec3 normalSample = texture(normalMap, fragUV).rgb;
    vec3 n = normalSample * 2.0 - 1.0;
    n.xy *= mat.normalScale;
    vec3 N = normalize(TBN * n);
    vec3 V = normalize(ubo.camPos.xyz - fragPos);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);
    int numLights = int(ubo.globalParams.x);
    for(int i = 0; i < numLights; ++i) {
        vec3 L;
        vec3 radiance;
        
        if (ubo.lights[i].position.w < 0.5) { // Directional
            L = normalize(-ubo.lights[i].direction.xyz);
            radiance = ubo.lights[i].color.rgb * ubo.lights[i].color.a;
        } else { // Point
            vec3 lightDir = ubo.lights[i].position.xyz - fragPos;
            float distance = length(lightDir);
            L = normalize(lightDir);
            float attenuation = 1.0 / (distance * distance);
            radiance = ubo.lights[i].color.rgb * ubo.lights[i].color.a * attenuation;
            
            if (distance > ubo.lights[i].params.x) radiance *= 0.0;
        }
        
        Lo += calculatePBR(L, V, N, radiance, albedo, metallic, roughness, F0);
    }

    vec3 ambient = vec3(0.03) * albedo * ao; 
    vec3 color = ambient + Lo + emissive;

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));

    outColor = vec4(color, 1.0);
}