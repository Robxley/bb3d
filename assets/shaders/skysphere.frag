#version 450

layout(location = 0) in vec3 fragDir;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D equirectangularMap;

layout(push_constant) uniform PushConstants {
    int flipY;
} pc;

const vec2 invAtan = vec2(0.1591, 0.3183);

vec2 SampleSphericalMap(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main() {
    vec2 uv = SampleSphericalMap(normalize(fragDir));
    
    if (pc.flipY != 0) {
        uv.y = 1.0 - uv.y;
    }
    
    // Explicit derivatives to fix the seam where atan() wraps from PI to -PI
    vec2 dx = dFdx(uv);
    vec2 dy = dFdy(uv);
    if (abs(dx.x) > 0.5) dx.x = dx.x > 0.0 ? dx.x - 1.0 : dx.x + 1.0;
    if (abs(dy.x) > 0.5) dy.x = dy.x > 0.0 ? dy.x - 1.0 : dy.x + 1.0;

    vec3 color = textureGrad(equirectangularMap, uv, dx, dy).rgb;
    
    outColor = vec4(color, 1.0);
}


