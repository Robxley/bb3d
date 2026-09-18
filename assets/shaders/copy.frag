#version 450

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform sampler2D sourceImage;
layout (set = 0, binding = 1) uniform PostProcessUBO {
    float exposure;
    float gamma;
    int enableTonemapping;
    int enableGammaCorrection;
} pp;

// ACES Filmic Tonemapping
vec3 ACESFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

void main() 
{
    vec4 color = texture(sourceImage, inUV);
    vec3 rgb = color.rgb * pp.exposure;

    if (pp.enableTonemapping == 1) {
        rgb = ACESFilm(rgb);
    }

    if (pp.enableGammaCorrection == 1) {
        rgb = pow(rgb, vec3(1.0 / pp.gamma));
    }

    outColor = vec4(rgb, color.a);
}


