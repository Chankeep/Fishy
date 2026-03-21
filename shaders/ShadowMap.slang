// ShadowMap Shader — Depth-only pass for shadow atlas generation

import Common;

struct VSOutput {
    float4 pos : SV_Position;
};

// ============================================================================
// Vertex Shader
// ============================================================================

[shader("vertex")]
VSOutput vertMain(VSInput input, uniform PushConstants pc, uint instanceID : SV_StartInstanceLocation) {
    InstanceData inst = pc.instanceDataPtr[instanceID];
    ShadowData shadow = pc.shadowDataPtr[pc.shadowRenderIndex];
    float4 worldPos = mul(inst.model, float4(input.inPosition, 1.0));
    VSOutput output;
    output.pos = mul(shadow.lightSpaceMatrix, worldPos);
    return output;
}

[shader("fragment")]
void fragMain() { /* Depth auto-written */ }