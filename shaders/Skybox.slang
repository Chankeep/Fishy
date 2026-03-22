// Skybox Shader — Renders a cubemap as the scene background

import Common;

// ============================================================================
// Skybox-specific Output
// ============================================================================

struct VSOutput {
    float4 pos : SV_Position;
    float3 localPos; // Direction vector for cubemap sampling
};

// IBL prefiltered environment map (Set 0, binding 1)
[[vk::binding(1, 0)]] SamplerCube prefilteredEnvMap;

// ============================================================================
// Vertex Shader
// ============================================================================

[shader("vertex")]
VSOutput vertMain(VSInput input, uniform PushConstants pc) {
    VSOutput output;
    
    // Pass local position as cubemap sample direction
    output.localPos = input.inPosition;
    
    // Remove translation from view matrix, keep rotation only
    float4x4 rotView = pc.globalDataPtr.view;
    rotView[0][3] = 0.0;
    rotView[1][3] = 0.0;
    rotView[2][3] = 0.0;
    
    float4 clipPos = mul(pc.globalDataPtr.proj, mul(rotView, float4(input.inPosition, 1.0)));
    
    // Set z = w so depth is always 1.0 (farthest)
    output.pos = clipPos.xyww;
    
    return output;
}

// ============================================================================
// Fragment Shader
// ============================================================================

[shader("fragment")]
float4 fragMain(VSOutput input) : SV_Target {
    float3 envColor = prefilteredEnvMap.SampleLevel(normalize(input.localPos), 0.0).rgb;
    
    // Tone mapping (Reinhard)
    envColor = envColor / (envColor + float3(1.0));
    
    return float4(envColor, 1.0);
}