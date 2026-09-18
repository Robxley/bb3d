#version 450

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform MaterialUBO {
    vec4 color;
} mat;

layout(set = 1, binding = 1) uniform sampler2D texSampler;

void main() {
    vec4 texColor = texture(texSampler, fragUV);
    // Ignore fragColor if it's black (fallback to white)
    vec3 fColor = (length(fragColor) < 0.01) ? vec3(1.0) : fragColor;
    // Ignore mat.color if it's black (fallback to white)
    vec3 mColor = (length(mat.color.rgb) < 0.01) ? vec3(1.0) : mat.color.rgb;
    
    vec3 finalRGB = texColor.rgb * fColor * mColor;
    float alpha = 1.0; // Force opaque

    outColor = vec4(finalRGB, alpha);
}


