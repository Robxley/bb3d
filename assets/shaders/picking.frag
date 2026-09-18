#version 450

// Output entity ID as a uint (R32_UINT format)
layout(location = 0) out uint outEntityID;

// Push constant with entity ID
layout(push_constant) uniform PushConstants {
    uint entityID;
} pc;

void main() {
    outEntityID = pc.entityID;
}


