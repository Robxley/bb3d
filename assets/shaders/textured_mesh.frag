#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D texSampler;

void main() {
    vec4 texColor = texture(texSampler, fragUV);
    // Combinaison de la couleur du sommet (pour les objets non texturés utilisant une texture blanche)
    // et de la texture (pour les objets texturés).
    // Une lumière basique est simulée via la normale.
    
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    float diff = max(dot(fragNormal, lightDir), 0.3); // 0.3 ambiante

    outColor = vec4(fragColor * texColor.rgb * diff, 1.0);
}