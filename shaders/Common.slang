// Common.slang — Shared structs, constants, and utilities for the Fishy Engine
// All shaders import this module for consistent data layout definitions.

// ============================================================================
// Constants
// ============================================================================

static const uint INVALID_TEXTURE_INDEX = 0xFFFFFFFF;
static const float M_PI = 3.141592653589793;
static const float MIN_ROUGHNESS = 0.04;

// ============================================================================
// Data Structures (must match C++ side exactly)
// ============================================================================

struct InstanceData {
    // Transform data
    float4x4 model;

    // --- Material Factors (Packed into vec4s for alignment) ---
    float4 baseColorFactor;       // Data 0: Base Color (RGBA)
    float4 emissiveFactor;        // Data 1: Emissive (RGB) + Strength (A)
    float4 pbrFactors;            // Data 2: x=metallic, y=roughness, z=normalScale, w=occlusionStrength
    float4 extraFactors1;         // Data 3: x=alphaCutoff, y=transmission, z=ior, w=clearcoatFactor
    float4 extraFactors2;         // Data 4: x=clearcoatRoughness, yzw=unused

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

struct GlobalUBO {
    float4x4 view;
    float4x4 proj;
    float4 camPos;      // xyz = camera position, w = unused

    uint   lightCount;
    float  debugViewInputs;    // 0=off, 1=baseColor, 2=normal, 3=AO, 4=emissive, 5=metallic, 6=roughness
    float  debugViewEquation;  // 0=off, 1=diffuse, 2=F, 3=G, 4=D, 5=specular
    float  prefilteredMipLevels;

    float  iblIntensity;
    float3 _pad;
};

struct LightData {
    float4 positionAndType;      // xyz=pos, w=type(0=dir,1=point,2=spot)
    float4 directionAndRange;    // xyz=dir (ray direction from source), w=range
    float4 colorAndIntensity;    // xyz=color, w=intensity
    float4 spotAngles;           // x=innerCos, y=outerCos, z=shadowIndex(-1=none), w=unused
};

struct ShadowData {
    float4x4 lightSpaceMatrix;
    float4   atlasRegion;        // xy=offset(0~1), zw=scale(0~1)
};

struct PushConstants {
    InstanceData* instanceDataPtr;
    GlobalUBO*    globalDataPtr;
    LightData*    lightDataPtr;
    ShadowData*   shadowDataPtr;
    uint          shadowRenderIndex;
    uint          _pad;
};

struct VSInput {
    [[vk::location(0)]] float3 inPosition;
    [[vk::location(1)]] float3 inNormal;
    [[vk::location(2)]] float2 inTexCoord;
    [[vk::location(3)]] float4 inTangent; // xyz = tangent, w = handedness
};

// ============================================================================
// Color Space Conversion
// ============================================================================

float4 sRGBToLinear(float4 srgb) {
    return float4(pow(srgb.rgb, float3(2.2)), srgb.a);
}

float3 linearToSRGB(float3 linear) {
    return pow(linear, float3(1.0 / 2.2));
}

// ============================================================================
// Light Direction Helper
// ============================================================================

// Get L vector (from surface toward light) for any light type.
// directionAndRange.xyz stores the ray direction (from source toward scene),
// so directional lights need negation.
float3 getLightDirection(LightData light, float3 worldPos) {
    uint lightType = uint(light.positionAndType.w);
    if (lightType == 0) {
        return -normalize(light.directionAndRange.xyz);
    } else {
        return normalize(light.positionAndType.xyz - worldPos);
    }
}
