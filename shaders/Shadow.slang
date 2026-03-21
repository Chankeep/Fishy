// Shadow.slang — Shadow atlas sampling with PCF filtering

import Common;

// ============================================================================
// Shadow Atlas Sampling
// ============================================================================

static const float SHADOW_ATLAS_SIZE_F = 4096.0;

// shadowMapSampler is passed from the main shader to avoid duplicate binding declarations
float calculateShadowAtlas(Sampler2D shadowMapSampler, ShadowData shadow, float3 worldPos, float3 N, float3 L)
{
    // Surface facing away from the light is naturally unlit — no shadow needed
    if (dot(N, L) <= 0.0) return 0.0;

    float4 fragPosLightSpace = mul(shadow.lightSpaceMatrix, float4(worldPos, 1.0));
    float3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    // Map to [0,1] then scale to atlas tile region
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    projCoords.xy = projCoords.xy * shadow.atlasRegion.zw + shadow.atlasRegion.xy;

    // Boundary check against tile region
    float2 tileMin = shadow.atlasRegion.xy;
    float2 tileMax = shadow.atlasRegion.xy + shadow.atlasRegion.zw;
    if (projCoords.z > 1.0 ||
        projCoords.x < tileMin.x || projCoords.x > tileMax.x ||
        projCoords.y < tileMin.y || projCoords.y > tileMax.y) {
        return 0.0;
    }

    float bias = max(0.005 * (1.0 - dot(N, L)), 0.001);

    // PCF 3x3 with atlas texel size, clamped to tile boundary
    float2 texelSize = 1.0 / float2(SHADOW_ATLAS_SIZE_F, SHADOW_ATLAS_SIZE_F);
    float shadowVal = 0.0;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            float2 sampleUV = clamp(
                projCoords.xy + float2(x, y) * texelSize,
                tileMin + texelSize, tileMax - texelSize);
            float closestDepth = shadowMapSampler.Sample(sampleUV).r;
            shadowVal += (projCoords.z - bias) > closestDepth ? 1.0 : 0.0;
        }
    }
    return shadowVal / 9.0;
}
