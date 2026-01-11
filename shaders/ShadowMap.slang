// ShadowMap Shader

struct VSInput {
    [[vk::location(0)]] float3 inPosition;
    [[vk::location(1)]] float3 inNormal;
    [[vk::location(2)]] float2 inTexCoord;
    [[vk::location(3)]] float4 inTangent;
};

struct VSOutput {
    float4 pos : SV_Position;
};

// ============================================================================
// Uniforms - 复用 Set 0 的 GlobalUBO
// ============================================================================

struct GlobalUBO {
    float4x4 view;
    float4x4 proj;
    float4x4 lightSpaceMatrix;
    float4 camPos;
    float4 lightDir;
    float4 lightColor;
    float debugViewInputs;
    float debugViewEquation;
    float prefilteredMipLevels;
    float iblIntensity;
};

struct InstanceData {
    // Transform data
    float4x4 model;

    // --- Material Factors (Packed into vec4s for alignment) ---
    // Data 0: Base Color (RGBA)
    float4 baseColorFactor;

    // Data 1: Emissive (RGB) + Strength (A)
    float4 emissiveFactor;

    // Data 2: PBR Standard
    // x: metallic, y: roughness, z: normalScale, w: occlusionStrength
    float4 pbrFactors;

    // Data 3: Advanced / Alpha
    // x: alphaCutoff, y: transmission, z: ior, w: clearcoatFactor
    float4 extraFactors1;

    // Data 4: Clearcoat details
    // x: clearcoatRoughness, y: unused, z: unused, w: unused
    float4 extraFactors2;

    // --- Bindless Indices (Integers) ---
    uint baseColorIndex;
    uint metallicRoughnessIndex;
    uint normalIndex;
    uint occlusionIndex;

    uint emissiveIndex;
    uint clearcoatIndex;
    uint clearcoatRoughnessIndex;
    uint clearcoatNormalIndex;

    uint transmissionIndex;
    uint flags;     // Bitfield for alphaMode, doubleSided
    uint objectID;  // Useful for mouse picking / debug
    uint _pad0;     // Align to 16 bytes end
};

struct PushConstants {
    InstanceData* instanceDataPtr;
    GlobalUBO* globalDataPtr;
};

// ============================================================================
// Vertex Shader
// ============================================================================

[shader("vertex")]
VSOutput vertMain(VSInput input, uniform PushConstants pc, uint instanceID : SV_StartInstanceLocation) {
    InstanceData inst = pc.instanceDataPtr[instanceID];
    float4 worldPos = mul(inst.model, float4(input.inPosition, 1.0));
    VSOutput output;
    output.pos = mul(pc.globalDataPtr.lightSpaceMatrix, worldPos);
    return output;
}
[shader("fragment")]
void fragMain() { /* Depth auto-written */ }