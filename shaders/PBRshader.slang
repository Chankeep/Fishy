// glTF PBR Shader with Multi-Light Support
// Based on Khronos glTF 2.0 PBR specification
// Supports: KHR_materials_clearcoat, KHR_materials_ior, KHR_materials_emissive_strength

import Common;
import BRDF;
import Shadow;
import NormalMapping;

// ============================================================================
// Descriptor Set Bindings
// ============================================================================

// IBL textures (Set 0)
[[vk::binding(0, 0)]] SamplerCube irradianceMap;
[[vk::binding(1, 0)]] SamplerCube prefilteredEnvMap;
[[vk::binding(2, 0)]] Sampler2D brdfLUT;
[[vk::binding(3, 0)]] Sampler2D shadowMap;

// Set 1: Bindless Texture Array
[[vk::binding(0, 1)]] Sampler2D g_Textures[];

// ============================================================================
// Vertex Shader Output (PBR-specific)
// ============================================================================

struct VSOutput {
    float4 pos : SV_Position;
    float3 worldPos;
    float2 texCoord;
    float3 worldNormal;
    float3 worldTangent;
    float tangentW;
    nointerpolation uint instanceID;
};

// ============================================================================
// Vertex Shader
// ============================================================================

[shader("vertex")]
VSOutput vertMain(VSInput input, uniform PushConstants pc, uint instanceID : SV_StartInstanceLocation) {
    InstanceData inst = pc.instanceDataPtr[instanceID];

    VSOutput output;
    float4 worldPos = mul(inst.model, float4(input.inPosition, 1.0));
    output.worldPos = worldPos.xyz;
    output.pos = mul(pc.globalDataPtr.proj, mul(pc.globalDataPtr.view, worldPos));
    output.texCoord = input.inTexCoord;
    
    float3x3 normalMat = (float3x3)inst.model;
    output.worldNormal = normalize(mul(normalMat, input.inNormal));
    output.worldTangent = normalize(mul(normalMat, input.inTangent.xyz));
    output.tangentW = input.inTangent.w;
    output.instanceID = instanceID;

    return output;
}

// ============================================================================
// Fragment Shader
// ============================================================================

[shader("fragment")]
float4 fragMain(VSOutput vertIn, uniform PushConstants pc) : SV_Target
{
    float2 uv = vertIn.texCoord;
    InstanceData inst = pc.instanceDataPtr[vertIn.instanceID];
    
    // ========================================================================
    // Material Sampling
    // ========================================================================
    
    float4 baseColorSample = sampleBindless(g_Textures, inst.baseColorIndex, uv, float4(1.0));
    float4 baseColor = sRGBToLinear(baseColorSample) * inst.baseColorFactor;
    
    // Alpha handling
    uint alphaMode = (inst.flags >> 1) & 0x3;
    if (alphaMode == 1) { // MASK mode
        if (baseColor.a < inst.extraFactors1.x) { discard; }
        baseColor.a = 1.0;
    }
    
    // Metallic-Roughness
    float4 mrSample = sampleBindless(g_Textures, inst.metallicRoughnessIndex, uv, float4(1.0, 1.0, 1.0, 1.0));
    float perceptualRoughness = clamp(mrSample.g * inst.pbrFactors.y, MIN_ROUGHNESS, 1.0);
    float metallic = clamp(mrSample.b * inst.pbrFactors.x, 0.0, 1.0);
    float alphaRoughness = perceptualRoughness * perceptualRoughness;
    
    // Reflectance
    float3 f0 = float3(0.04);
    float3 diffuseColor = baseColor.rgb * (float3(1.0) - f0) * (1.0 - metallic);
    float3 specularColor = lerp(f0, baseColor.rgb, metallic);
    
    float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);
    float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
    float3 specularEnvironmentR0 = specularColor;
    
    // Normal
    bool hasNormalMap = (inst.flags & (1 << 5)) != 0 && inst.normalIndex != INVALID_TEXTURE_INDEX;
    float3 n = hasNormalMap 
        ? getNormalBindless(g_Textures, vertIn.worldPos, uv, vertIn.worldNormal, vertIn.worldTangent, vertIn.tangentW, inst) 
        : normalize(vertIn.worldNormal);
    float3 v = normalize(pc.globalDataPtr.camPos.xyz - vertIn.worldPos);
    float3 reflection = normalize(reflect(-v, n));
    float NdotV = clamp(abs(dot(n, v)), 0.001, 1.0);

    // ========================================================================
    // Multi-Light Loop: accumulate direct lighting with per-light shadows
    // ========================================================================
    
    uint lightCount = pc.globalDataPtr.lightCount;
    float3 color = float3(0.0);
    for (uint si = 0; si < lightCount; si++) {
        LightData sLight = pc.lightDataPtr[si];
        float3 contrib = evaluateLight(sLight, vertIn.worldPos, n, v,
                                       diffuseColor, specularColor,
                                       specularEnvironmentR0, reflectance90,
                                       alphaRoughness, NdotV);

        int shadowIdx = int(sLight.spotAngles.z);
        if (shadowIdx >= 0) {
            ShadowData sd = pc.shadowDataPtr[shadowIdx];
            float3 L = getLightDirection(sLight, vertIn.worldPos);
            float sf = calculateShadowAtlas(shadowMap, sd, vertIn.worldPos, n, L);
            contrib *= (1.0 - sf * 0.9);
        }

        color += contrib;
    }
    
    // ========================================================================
    // Clearcoat Layer
    // ========================================================================
    
    float clearcoatFactor = inst.extraFactors1.w;
    if (clearcoatFactor > 0.0) {
        float4 ccSample = sampleBindless(g_Textures, inst.clearcoatIndex, uv, float4(1.0));
        float clearcoatStrength = ccSample.r * clearcoatFactor;
        float4 ccrSample = sampleBindless(g_Textures, inst.clearcoatRoughnessIndex, uv, float4(1.0));
        float clearcoatRoughness = clamp(ccrSample.g * inst.extraFactors2.x, MIN_ROUGHNESS, 1.0);
        
        float3 clearcoatN = getClearcoatNormalBindless(g_Textures, vertIn.worldPos, uv,
            vertIn.worldNormal, vertIn.worldTangent, vertIn.tangentW, inst, normalize(vertIn.worldNormal));
        
        float3 clearcoatTotal = float3(0.0);
        for (uint i = 0; i < lightCount; i++) {
            LightData light = pc.lightDataPtr[i];
            float3 lightColor = light.colorAndIntensity.xyz * light.colorAndIntensity.w;
            float3 ccL = getLightDirection(light, vertIn.worldPos);
            float ccAtten = getLightAttenuation(light, vertIn.worldPos);
            clearcoatTotal += evaluateClearcoatBRDF(clearcoatN, v, ccL, clearcoatRoughness) * lightColor * ccAtten;
        }
        
        float clearcoatNdotV = max(dot(clearcoatN, v), 0.0);
        float clearcoatFresnel = 0.04 + (1.0 - 0.04) * pow(1.0 - clearcoatNdotV, 5.0);
        color = color * (1.0 - clearcoatStrength * clearcoatFresnel) + clearcoatTotal * clearcoatStrength;
    }
    
    // ========================================================================
    // Ambient Occlusion
    // ========================================================================
    
    bool hasOcclusion = (inst.flags & (1 << 6)) != 0 && inst.occlusionIndex != INVALID_TEXTURE_INDEX;
    if (hasOcclusion) {
        float ao = sampleBindless(g_Textures, inst.occlusionIndex, uv, float4(1.0)).r;
        color = lerp(color, color * ao, inst.pbrFactors.w);
    }
    
    // ========================================================================
    // Image-Based Lighting (IBL)
    // ========================================================================
    
    float3 ambient = float3(0.0);
    if (pc.globalDataPtr.iblIntensity > 0.0) {
        float3 irradiance = irradianceMap.Sample(n).rgb;
        float3 diffuseIBL = diffuseColor * irradiance;
        
        float lod = perceptualRoughness * pc.globalDataPtr.prefilteredMipLevels;
        float3 prefilteredColor = prefilteredEnvMap.SampleLevel(reflection, lod).rgb;
        
        float2 brdf = brdfLUT.Sample(float2(NdotV, perceptualRoughness)).rg;
        float3 specularIBL = prefilteredColor * (specularColor * brdf.x + brdf.y);
        
        ambient = (diffuseIBL + specularIBL) * pc.globalDataPtr.iblIntensity;
    } else {
        ambient = float3(0.02) * baseColor.rgb;
    }
    
    // Apply combined shadow to ambient (max across all shadow casters)
    float combinedShadow = 0.0;
    for (uint ai = 0; ai < lightCount; ai++) {
        int aIdx = int(pc.lightDataPtr[ai].spotAngles.z);
        if (aIdx >= 0) {
            ShadowData asd = pc.shadowDataPtr[aIdx];
            float3 aL = getLightDirection(pc.lightDataPtr[ai], vertIn.worldPos);
            float sv = calculateShadowAtlas(shadowMap, asd, vertIn.worldPos, n, aL);
            combinedShadow = max(combinedShadow, sv);
        }
    }
    color += ambient * (1.0 - combinedShadow * 0.9);
    
    // ========================================================================
    // Emissive
    // ========================================================================
    
    float emissiveStrength = inst.emissiveFactor.w;
    float3 emissive = inst.emissiveFactor.rgb * emissiveStrength;
    if (inst.emissiveIndex != INVALID_TEXTURE_INDEX) {
        emissive *= sRGBToLinear(sampleBindless(g_Textures, inst.emissiveIndex, uv, float4(0.0))).rgb;
    }
    color += emissive;
    
    // ========================================================================
    // Debug Visualization
    // ========================================================================
    
    if (pc.globalDataPtr.debugViewInputs > 0.0) {
        int index = int(pc.globalDataPtr.debugViewInputs);
        switch (index) {
            case 1: color = baseColor.rgb; break;
            case 2: color = n * 0.5 + 0.5; break;
            case 3: color = hasOcclusion ? sampleBindless(g_Textures, inst.occlusionIndex, uv, float4(1.0)).rrr : float3(1.0); break;
            case 4: color = emissive; break;
            case 5: color = sampleBindless(g_Textures, inst.metallicRoughnessIndex, uv, float4(1.0)).bbb; break;
            case 6: color = sampleBindless(g_Textures, inst.metallicRoughnessIndex, uv, float4(1.0)).ggg; break;
            case 7: color = float3(uv, 0.0); break;
            case 8: color = normalize(vertIn.worldNormal); break;
            case 9:
                {
                    float3x3 TBN = getTBN(vertIn.worldPos, uv, vertIn.worldNormal, vertIn.worldTangent, vertIn.tangentW);
                    color = TBN[0]*0.5+0.5;
                }
                break;
            case 10:
                {
                    float3x3 TBN = getTBN(vertIn.worldPos, uv, vertIn.worldNormal, vertIn.worldTangent, vertIn.tangentW);
                    color = TBN[1]*0.5+0.5;
                }
                break;
            case 11: // Shadow debug: R=shadow0, G=shadow1, B=shadow2
                {
                    float3 shadowVals = float3(0.0);
                    for (uint di = 0; di < lightCount; di++) {
                        int sIdx = int(pc.lightDataPtr[di].spotAngles.z);
                        if (sIdx >= 0 && sIdx < 3) {
                            ShadowData sd = pc.shadowDataPtr[sIdx];
                            float3 dL = getLightDirection(pc.lightDataPtr[di], vertIn.worldPos);
                            float sv = calculateShadowAtlas(shadowMap, sd, vertIn.worldPos, n, dL);
                            if (sIdx == 0) shadowVals.r = sv;
                            else if (sIdx == 1) shadowVals.g = sv;
                            else shadowVals.b = sv;
                        }
                    }
                    color = float3(1.0) - shadowVals;
                }
                break;
        }
        return float4(color, baseColor.a);
    }
    
    // PBR equation debug
    if (pc.globalDataPtr.debugViewEquation > 0.0 && lightCount > 0) {
        LightData dbgLight = pc.lightDataPtr[0];
        float3 dbgL = getLightDirection(dbgLight, vertIn.worldPos);
        float3 dbgH = normalize(dbgL + v);
        
        PBRInfo dbgPbr;
        dbgPbr.NdotL = clamp(dot(n, dbgL), 0.001, 1.0);
        dbgPbr.NdotV = NdotV;
        dbgPbr.NdotH = clamp(dot(n, dbgH), 0.0, 1.0);
        dbgPbr.LdotH = clamp(dot(dbgL, dbgH), 0.0, 1.0);
        dbgPbr.VdotH = clamp(dot(v, dbgH), 0.0, 1.0);
        dbgPbr.reflectance0 = specularEnvironmentR0;
        dbgPbr.reflectance90 = float3(reflectance90);
        dbgPbr.alphaRoughness = alphaRoughness;
        dbgPbr.diffuseColor = diffuseColor;
        dbgPbr.specularColor = specularColor;
        
        float3 dbgF = specularReflection(dbgPbr);
        float dbgG = geometricOcclusion(dbgPbr);
        float dbgD = microfacetDistribution(dbgPbr);
        float3 dbgDiffuseContrib = (float3(1.0) - dbgF) * diffuseBRDF(diffuseColor);
        float3 dbgSpecContrib = dbgF * dbgG * dbgD / (4.0 * dbgPbr.NdotL * dbgPbr.NdotV);
        
        int eqIndex = int(pc.globalDataPtr.debugViewEquation);
        switch (eqIndex) {
            case 1: color = dbgDiffuseContrib; break;
            case 2: color = dbgF; break;
            case 3: color = float3(dbgG); break;
            case 4: color = float3(dbgD); break;
            case 5: color = dbgSpecContrib; break;
        }
        color = linearToSRGB(color);
        return float4(color, baseColor.a);
    }

    // ========================================================================
    // Tone mapping and gamma correction
    // ========================================================================
    
    color = color / (color + float3(1.0));
    color = linearToSRGB(color);
    
    return float4(color, baseColor.a);
}
