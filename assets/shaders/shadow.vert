#version 450
#extension GL_EXT_nonuniform_qualifier : enable

// Input attributes (using only position for depth pass)
layout(location = 0) in vec3 inPosition;

// We do not need the other locations for casting a shadow

// Push constants for shadow pass
layout(push_constant) uniform PushConstants {
    mat4 lightVP; // L'espace lumière-projection combiné pour la cascade actuelle
} pc;

// SSBO for instancing (matches global layout)
layout(set = 0, binding = 1) readonly buffer InstanceBuffer {
    mat4 models[];
} instances;

void main() {
    mat4 model = instances.models[gl_InstanceIndex];
    gl_Position = pc.lightVP * model * vec4(inPosition, 1.0);
}


