// NormalMapping.slang — TBN matrix and normal map sampling utilities

import Common;

// ============================================================================
// Helper: Sample bindless texture or return default
// Requires g_Textures to be passed from main shader
// ============================================================================

float4 sampleBindless(Sampler2D textures[], uint texIndex, float2 uv, float4 defaultValue) {
    if (texIndex == INVALID_TEXTURE_INDEX) {
        return defaultValue;
    }
    return textures[NonUniformResourceIndex(texIndex)].Sample(uv);
}

// ============================================================================
// TBN Matrix
// ============================================================================

// Calculate the TBN matrix using either vertex tangents or screen-space derivatives
// Reference: http://www.thetenthplanet.de/archives/1180
float3x3 getTBN(float3 worldPos, float2 texCoord, float3 worldNormal, float3 worldTangent, float tangentW) {
    float3 N = normalize(worldNormal);
    float3 T, B;
    
    // Check if we have valid tangent data (tangentW != 0 indicates valid tangent)
    if (abs(tangentW) > 0.001) {
        T = normalize(worldTangent);
        T = normalize(T - dot(T, N) * N); // Gram-Schmidt re-orthogonalization
        B = cross(N, T) * tangentW;
    } else {
        // Perturb normal using screen-space derivatives (no tangent provided)
        float3 q1 = ddx(worldPos);
        float3 q2 = ddy(worldPos);
        float2 st1 = ddx(texCoord);
        float2 st2 = ddy(texCoord);
        
        float3 t = normalize((q1 * st2.y - q2 * st1.y) / (st1.x * st2.y - st1.y * st2.x));

        T = normalize(t - N * dot(N, t));
        B = cross(N, T);
    }
    
    return float3x3(T, B, N);
}

// ============================================================================
// Normal Map Sampling
// ============================================================================

// Get normal from bindless normal map
float3 getNormalBindless(Sampler2D textures[], float3 worldPos, float2 texCoord,
                         float3 worldNormal, float3 worldTangent, float tangentW,
                         InstanceData inst) {
    float4 normalSample = sampleBindless(textures, inst.normalIndex, texCoord, float4(0.5, 0.5, 1.0, 1.0));
    float3 tangentNormal = normalSample.rgb * 2.0 - 1.0;
    tangentNormal.xy *= inst.pbrFactors.z; // normalScale
    tangentNormal = normalize(tangentNormal);
    
    return normalize(mul(tangentNormal, getTBN(worldPos, texCoord, worldNormal, worldTangent, tangentW)));
}

// Get clearcoat normal (separate from base normal)
float3 getClearcoatNormalBindless(Sampler2D textures[], float3 worldPos, float2 texCoord,
                                  float3 worldNormal, float3 worldTangent, float tangentW,
                                  InstanceData inst, float3 geometricNormal) {
    if (inst.clearcoatNormalIndex == INVALID_TEXTURE_INDEX) {
        return geometricNormal;
    }
    
    float4 normalSample = sampleBindless(textures, inst.clearcoatNormalIndex, texCoord, float4(0.5, 0.5, 1.0, 1.0));
    float3 tangentNormal = normalSample.rgb * 2.0 - 1.0;
    
    return normalize(mul(tangentNormal, getTBN(worldPos, texCoord, worldNormal, worldTangent, tangentW)));
}
