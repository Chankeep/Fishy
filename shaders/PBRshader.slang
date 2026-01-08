// glTF PBR Shader with Extensions
// Based on Khronos glTF 2.0 PBR specification and WebGL PBR implementation
// See https://github.com/KhronosGroup/glTF-WebGL-PBR
// Supports: KHR_materials_clearcoat, KHR_materials_ior, KHR_materials_emissive_strength

struct VSInput {
    [[vk::location(0)]] float3 inPosition;
    [[vk::location(1)]] float3 inNormal;
    [[vk::location(2)]] float2 inTexCoord;
    [[vk::location(3)]] float4 inTangent; // xyz = tangent, w = handedness
};

// Set 0: Global Data
struct GlobalUBO {
    float4x4 view;
    float4x4 proj;
    float4 camPos;      // xyz = camera position, w = unused
    float4 lightDir;    // xyz = light direction, w = unused
    float4 lightColor;  // xyz = light color, w = unused
    // Debug visualization controls
    float debugViewInputs;   // 0=off, 1=baseColor, 2=normal, 3=AO, 4=emissive, 5=metallic, 6=roughness
    float debugViewEquation; // 0=off, 1=diffuse, 2=F, 3=G, 4=D, 5=specular
    // IBL parameters
    float prefilteredMipLevels;
    float iblIntensity;
};


// IBL textures (Set 0)
[[vk::binding(0, 0)]] SamplerCube irradianceMap;
[[vk::binding(1, 0)]] SamplerCube prefilteredEnvMap;
[[vk::binding(2, 0)]] Sampler2D brdfLUT;

// Set 1: Bindless Texture Array
[[vk::binding(0, 1)]] Sampler2D g_Textures[];

// Set 2: Bindless Instance Data (SSBO Array)
// Each instance contains transforms, texture indices, and material properties
static const uint INVALID_TEXTURE_INDEX = 0xFFFFFFFF;


struct InstanceData {
    // Transform data
    float4x4 model;
    float4x4 normalMatrix;

    // Bindless texture indices (UINT32_MAX = no texture)
    uint baseColorIndex;
    uint metallicRoughnessIndex;
    uint normalIndex;
    uint occlusionIndex;
    uint emissiveIndex;
    uint clearcoatIndex;
    uint clearcoatRoughnessIndex;
    uint clearcoatNormalIndex;
    uint transmissionIndex;
    uint _padding0[3]; // Align to 16 bytes for float4 baseColorFactor

    // Material properties (inline)
    float4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    float normalScale;
    float occlusionStrength;
    float4 emissiveFactor;  // RGB + strength in w
    float alphaCutoff;
    uint flags;
    float clearcoatFactor;
    float clearcoatRoughnessFactor;
    float transmissionFactor; // KHR_materials_transmission
    float ior;                // KHR_materials_ior
};
// [[vk::binding(0, 2)]] StructuredBuffer<InstanceData> g_Instances;


// Slang uses C-style pointers for Buffer Device Address
// Push constants are passed as uniform parameters to entry points
struct PushConstants {
    InstanceData* instanceDataPtr;
    GlobalUBO* globalDataPtr;
};

struct VSOutput {
    float4 pos : SV_Position;
    float3 worldPos;
    float2 texCoord;
    float3 worldNormal;
    float3 worldTangent;
    float tangentW;
    nointerpolation uint instanceID; // Pass to fragment shader for bindless lookup
};

// Helper: Sample bindless texture or return default
float4 sampleBindless(uint texIndex, float2 uv, float4 defaultValue) {
    if (texIndex == INVALID_TEXTURE_INDEX) {
        return defaultValue;
    }
    return g_Textures[NonUniformResourceIndex(texIndex)].Sample(uv);
}

// Constants
static const float M_PI = 3.141592653589793;
static const float MIN_ROUGHNESS = 0.04;

// Encapsulate the various inputs used by the various functions in the shading equation
// We store values in this struct to simplify the integration of alternative implementations
struct PBRInfo {
    float NdotL;                  // cos angle between normal and light direction
    float NdotV;                  // cos angle between normal and view direction
    float NdotH;                  // cos angle between normal and half vector
    float LdotH;                  // cos angle between light direction and half vector
    float VdotH;                  // cos angle between view direction and half vector
    float perceptualRoughness;    // roughness value, as authored by the model creator
    float metalness;              // metallic value at the surface
    float3 reflectance0;          // full reflectance color (normal incidence angle)
    float3 reflectance90;         // reflectance color at grazing angle
    float alphaRoughness;         // roughness mapped to a more linear change in the roughness
    float3 diffuseColor;          // color contribution from diffuse lighting
    float3 specularColor;         // color contribution from specular lighting
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
// Normal Mapping
// ============================================================================

// Calculate the TBN matrix using either vertex tangents or screen-space derivatives
// Reference: http://www.thetenthplanet.de/archives/1180
float3x3 getTBN(VSOutput vertIn) {
    float3 N = normalize(vertIn.worldNormal);
    float3 T, B;
    
    // Check if we have valid tangent data (tangentW != 0 indicates valid tangent)
    if (abs(vertIn.tangentW) > 0.001) {
        // Use provided tangent
        T = normalize(vertIn.worldTangent);
        T = normalize(T - dot(T, N) * N); // Gram-Schmidt re-orthogonalization
        B = cross(N, T) * vertIn.tangentW;
    } else {
        // Perturb normal using screen-space derivatives (no tangent provided)
        // This method computes the cotangent frame from screen-space derivatives
        float3 q1 = ddx(vertIn.worldPos);
        float3 q2 = ddy(vertIn.worldPos);
        float2 st1 = ddx(vertIn.texCoord);
        float2 st2 = ddy(vertIn.texCoord);
        
        // Compute tangent following the reference GLSL implementation
        // T = normalize(q1 * st2.t - q2 * st1.t) where .t is .y in GLSL
        float3 t = normalize((q1 * st2.y - q2 * st1.y) / (st1.x * st2.y - st1.y * st2.x));


        T = normalize(t - N * dot(N, t));
        B = cross(N, T);
    }
    
    // In Slang/HLSL, float3x3(row0, row1, row2) creates a matrix with these as rows.
    // We want: result = TBN * tangentNormal (column vector, GLSL style)
    // Using mul(tangentNormal, TBN) with tangentNormal as row vector gives:
    // [tx, ty, tz] * | T.x T.y T.z |   = tx*T + ty*B + tz*N
    //                | B.x B.y B.z |
    //                | N.x N.y N.z |
    // This is correct transformation from tangent space to world space.
    return float3x3(T, B, N);
}

// Get normal from bindless normal map
float3 getNormalBindless(VSOutput vertIn, InstanceData inst) {
    float4 normalSample = sampleBindless(inst.normalIndex, vertIn.texCoord, float4(0.5, 0.5, 1.0, 1.0));
    float3 tangentNormal = normalSample.rgb * 2.0 - 1.0;
    tangentNormal.xy *= inst.normalScale;
    tangentNormal = normalize(tangentNormal);
    
    return normalize(mul(tangentNormal, getTBN(vertIn)));
}

// Get clearcoat normal (separate from base normal)
float3 getClearcoatNormalBindless(VSOutput vertIn, InstanceData inst, float3 geometricNormal) {
    // Check if clearcoat normal texture is present
    if (inst.clearcoatNormalIndex == INVALID_TEXTURE_INDEX) {
        return geometricNormal;
    }
    
    float4 normalSample = sampleBindless(inst.clearcoatNormalIndex, vertIn.texCoord, float4(0.5, 0.5, 1.0, 1.0));
    float3 tangentNormal = normalSample.rgb * 2.0 - 1.0;
    
    return normalize(mul(tangentNormal, getTBN(vertIn)));
}

// ============================================================================
// BRDF Functions
// ============================================================================

// Basic Lambertian diffuse
// Implementation from Lambert's Photometria https://archive.org/details/lambertsphotome00lambgoog
// See also [1], Equation 1
float3 diffuse(PBRInfo pbrInputs) {
    return pbrInputs.diffuseColor / M_PI;
}

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel from [4], Equation 15
float3 specularReflection(PBRInfo pbrInputs) {
    return pbrInputs.reflectance0 + (pbrInputs.reflectance90 - pbrInputs.reflectance0) * 
           pow(clamp(1.0 - pbrInputs.VdotH, 0.0, 1.0), 5.0);
}

// This calculates the specular geometric attenuation (aka G()),
// where rougher material will reflect less light back to the viewer.
// This implementation is based on [1] Equation 4, and we adopt their modifications to
// alphaRoughness as input as originally proposed in [2].
float geometricOcclusion(PBRInfo pbrInputs) {
    float NdotL = pbrInputs.NdotL;
    float NdotV = pbrInputs.NdotV;
    float r = pbrInputs.alphaRoughness;

    float attenuationL = 2.0 * NdotL / (NdotL + sqrt(r * r + (1.0 - r * r) * (NdotL * NdotL)));
    float attenuationV = 2.0 * NdotV / (NdotV + sqrt(r * r + (1.0 - r * r) * (NdotV * NdotV)));
    return attenuationL * attenuationV;
}

// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" 
// by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float microfacetDistribution(PBRInfo pbrInputs) {
    float roughnessSq = pbrInputs.alphaRoughness * pbrInputs.alphaRoughness;
    float f = (pbrInputs.NdotH * roughnessSq - pbrInputs.NdotH) * pbrInputs.NdotH + 1.0;
    return roughnessSq / (M_PI * f * f);
}

// Calculate F0 from IOR
float3 F0FromIOR(float ior) {
    float f = (ior - 1.0) / (ior + 1.0);
    return float3(f * f);
}

// ============================================================================
// Clearcoat BRDF
// ============================================================================

// Clearcoat uses a simplified BRDF with F0 = 0.04 (dielectric)
float3 evaluateClearcoatBRDF(float3 N, float3 V, float3 L, float clearcoatRoughness) {
    float3 H = normalize(V + L);
    
    float NdotL = max(dot(N, L), 0.001);
    float NdotV = max(dot(N, V), 0.001);
    float NdotH = clamp(dot(N, H), 0.0, 1.0);
    float VdotH = clamp(dot(V, H), 0.0, 1.0);
    
    if (NdotL <= 0.0) return float3(0.0);
    
    float alphaRoughness = clearcoatRoughness * clearcoatRoughness;
    
    // F0 = 0.04 for dielectric clearcoat
    float3 F0 = float3(0.04);
    float3 F = F0 + (float3(1.0) - F0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
    
    // D - GGX Distribution
    float roughnessSq = alphaRoughness * alphaRoughness;
    float f = (NdotH * roughnessSq - NdotH) * NdotH + 1.0;
    float D = roughnessSq / (M_PI * f * f);
    
    // G - Geometric attenuation
    float attenuationL = 2.0 * NdotL / (NdotL + sqrt(roughnessSq + (1.0 - roughnessSq) * (NdotL * NdotL)));
    float attenuationV = 2.0 * NdotV / (NdotV + sqrt(roughnessSq + (1.0 - roughnessSq) * (NdotV * NdotV)));
    float G = attenuationL * attenuationV;
    
    float3 specular = (D * G * F) / (4.0 * NdotL * NdotV);
    
    return specular * NdotL;
}

// ============================================================================
// Vertex Shader
// ============================================================================

[shader("vertex")]
VSOutput vertMain(VSInput input, uniform PushConstants pc, uint instanceID : SV_InstanceID) {
    // Get per-instance data via BDA pointer
    InstanceData inst = pc.instanceDataPtr[instanceID];

    VSOutput output;
    float4 worldPos = mul(inst.model, float4(input.inPosition, 1.0));
    output.worldPos = worldPos.xyz;
    output.pos = mul(pc.globalDataPtr.proj, mul(pc.globalDataPtr.view, worldPos));
    output.texCoord = input.inTexCoord;
    
    // Transform normal and tangent using precomputed normalMatrix
    float3x3 normalMat = (float3x3)inst.normalMatrix;
    output.worldNormal = normalize(mul(normalMat, input.inNormal));
    output.worldTangent = normalize(mul(normalMat, input.inTangent.xyz));
    output.tangentW = input.inTangent.w;
    
    // Pass instance ID to fragment shader for material lookup
    output.instanceID = instanceID;

    return output;
}

// Fragment Shader
[shader("fragment")]
float4 fragMain(VSOutput vertIn, uniform PushConstants pc) : SV_Target
{
    float2 uv = vertIn.texCoord;
    
    // Get instance data via BDA pointer
    InstanceData inst = pc.instanceDataPtr[vertIn.instanceID];
    
    // Sample textures using bindless array
    float4 baseColorSample = sampleBindless(inst.baseColorIndex, uv, float4(1.0));
    float4 baseColor = sRGBToLinear(baseColorSample) * inst.baseColorFactor;
    
    // Alpha handling
    uint alphaMode = (inst.flags >> 1) & 0x3;
    if (alphaMode == 1) { // MASK mode
        if (baseColor.a < inst.alphaCutoff) {
            discard;
        }
        baseColor.a = 1.0;
    }
    
    // glTF: metallic in B channel, roughness in G channel
    float4 mrSample = sampleBindless(inst.metallicRoughnessIndex, uv, float4(1.0, 1.0, 1.0, 1.0));
    float perceptualRoughness = clamp(mrSample.g * inst.roughnessFactor, MIN_ROUGHNESS, 1.0);
    float metallic = clamp(mrSample.b * inst.metallicFactor, 0.0, 1.0);
    
    // Roughness is authored as perceptual roughness; convert by squaring
    float alphaRoughness = perceptualRoughness * perceptualRoughness;
    
    // Calculate reflectance - use default IOR of 1.5
    float3 f0 = float3(0.04); // F0FromIOR(1.5)
    
    float3 diffuseColor = baseColor.rgb * (float3(1.0) - f0) * (1.0 - metallic);
    float3 specularColor = lerp(f0, baseColor.rgb, metallic);
    
    // Compute reflectance
    float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);
    float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
    float3 specularEnvironmentR0 = specularColor;
    float3 specularEnvironmentR90 = float3(reflectance90);
    
    // Get normal - check if normal map is present (bit 5 = HasNormalMap)
    bool hasNormalMap = (inst.flags & (1 << 5)) != 0 && inst.normalIndex != INVALID_TEXTURE_INDEX;
    float3 n = hasNormalMap ? getNormalBindless(vertIn, inst) : normalize(vertIn.worldNormal);
    float3 v = normalize(pc.globalDataPtr.camPos.xyz - vertIn.worldPos);
    float3 l = normalize(pc.globalDataPtr.lightDir.xyz);
    float3 h = normalize(l + v);
    float3 reflection = normalize(reflect(-v, n));

    float NdotL = clamp(dot(n, l), 0.001, 1.0);
    float NdotV = clamp(abs(dot(n, v)), 0.001, 1.0);
    float NdotH = clamp(dot(n, h), 0.0, 1.0);
    float LdotH = clamp(dot(l, h), 0.0, 1.0);
    float VdotH = clamp(dot(v, h), 0.0, 1.0);

    // ========================================================================
    // Build PBRInfo struct
    // ========================================================================
    
    PBRInfo pbrInputs;
    pbrInputs.NdotL = NdotL;
    pbrInputs.NdotV = NdotV;
    pbrInputs.NdotH = NdotH;
    pbrInputs.LdotH = LdotH;
    pbrInputs.VdotH = VdotH;
    pbrInputs.perceptualRoughness = perceptualRoughness;
    pbrInputs.metalness = metallic;
    pbrInputs.reflectance0 = specularEnvironmentR0;
    pbrInputs.reflectance90 = specularEnvironmentR90;
    pbrInputs.alphaRoughness = alphaRoughness;
    pbrInputs.diffuseColor = diffuseColor;
    pbrInputs.specularColor = specularColor;

    // ========================================================================
    // Calculate the shading terms for the microfacet specular shading model
    // ========================================================================
    
    float3 F = specularReflection(pbrInputs);
    float G = geometricOcclusion(pbrInputs);
    float D = microfacetDistribution(pbrInputs);

    // Calculation of analytical lighting contribution
    float3 diffuseContrib = (float3(1.0) - F) * diffuse(pbrInputs);
    float3 specContrib = F * G * D / (4.0 * NdotL * NdotV);
    
    // Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
    float3 color = NdotL * pc.globalDataPtr.lightColor.xyz * (diffuseContrib + specContrib);
    
    // Clearcoat layer
    float clearcoatFactor = inst.clearcoatFactor;
    if (clearcoatFactor > 0.0) {
        float4 ccSample = sampleBindless(inst.clearcoatIndex, uv, float4(1.0));
        float clearcoatStrength = ccSample.r * clearcoatFactor;
        float4 ccrSample = sampleBindless(inst.clearcoatRoughnessIndex, uv, float4(1.0));
        float clearcoatRoughness = clamp(ccrSample.g * inst.clearcoatRoughnessFactor, MIN_ROUGHNESS, 1.0);
        
        float3 clearcoatN = getClearcoatNormalBindless(vertIn, inst, normalize(vertIn.worldNormal));
        float3 clearcoatBRDF = evaluateClearcoatBRDF(clearcoatN, v, l, clearcoatRoughness) * pc.globalDataPtr.lightColor.xyz;
        
        // Fresnel for clearcoat energy conservation
        float clearcoatNdotV = max(dot(clearcoatN, v), 0.0);
        float clearcoatFresnel = 0.04 + (1.0 - 0.04) * pow(1.0 - clearcoatNdotV, 5.0);
        
        // Blend: clearcoat on top, attenuating base layer
        color = color * (1.0 - clearcoatStrength * clearcoatFresnel) + clearcoatBRDF * clearcoatStrength;
    }
    
    // Apply ambient occlusion
    bool hasOcclusion = (inst.flags & (1 << 6)) != 0 && inst.occlusionIndex != INVALID_TEXTURE_INDEX;
    if (hasOcclusion) {
        float ao = sampleBindless(inst.occlusionIndex, uv, float4(1.0)).r;
        color = lerp(color, color * ao, inst.occlusionStrength);
    }
    
    // ========================================================================
    // Image-Based Lighting (IBL)
    // ========================================================================
    
    float3 ambient = float3(0.0);
    if (pc.globalDataPtr.iblIntensity > 0.0) {
        // Diffuse IBL - sample irradiance map with normal
        float3 irradiance = irradianceMap.Sample(n).rgb;
        float3 diffuseIBL = diffuseColor * irradiance;
        
        // Specular IBL - sample pre-filtered environment with reflection and roughness-based LOD
        float lod = perceptualRoughness * pc.globalDataPtr.prefilteredMipLevels;
        float3 prefilteredColor = prefilteredEnvMap.SampleLevel(reflection, lod).rgb;
        
        // BRDF LUT lookup for split-sum approximation
        float2 brdf = brdfLUT.Sample(float2(NdotV, perceptualRoughness)).rg;
        float3 specularIBL = prefilteredColor * (specularColor * brdf.x + brdf.y);
        
        // Combine diffuse and specular IBL
        ambient = (diffuseIBL + specularIBL) * pc.globalDataPtr.iblIntensity;
    } else {
        // Fallback to simple ambient when IBL not available
        ambient = float3(0.03) * baseColor.rgb;
    }
    color += ambient;
    
    // Emissive
    float emissiveStrength = inst.emissiveFactor.w;
    float3 emissive = inst.emissiveFactor.rgb * emissiveStrength;
    if (inst.emissiveIndex != INVALID_TEXTURE_INDEX) {
        emissive *= sRGBToLinear(sampleBindless(inst.emissiveIndex, uv, float4(0.0))).rgb;
    }
    color += emissive;
    
    // Debug Visualization
    if (pc.globalDataPtr.debugViewInputs > 0.0) {
        int index = int(pc.globalDataPtr.debugViewInputs);
        switch (index) {
            case 1: // Base Color
                color = baseColor.rgb;
                break;
            case 2: // Shading Normal
                color = n * 0.5 + 0.5;
                break;
            case 3: // AO
                color = hasOcclusion ? sampleBindless(inst.occlusionIndex, uv, float4(1.0)).rrr : float3(1.0);
                break;
            case 4: // Emissive
                color = emissive;
                break;
            case 5: // Metallic
                color = sampleBindless(inst.metallicRoughnessIndex, uv, float4(1.0)).bbb;
                break;
            case 6: // Roughness
                color = sampleBindless(inst.metallicRoughnessIndex, uv, float4(1.0)).ggg;
                break;
            case 7: // UV
                color = float3(uv, 0.0);
                break;
            case 8: // Geometric Normal
                color = normalize(vertIn.worldNormal);
                break;
            case 9: // Tangent
                {
                    float3x3 TBN = getTBN(vertIn);
                    color = TBN[0]*0.5+0.5;
                }
                break;
            case 10: // Bitangent
                {
                    float3x3 TBN = getTBN(vertIn);
                    color = TBN[1]*0.5+0.5;
                }
                break;
        }
        
        return float4(color, baseColor.a);
    }
    
    // PBR equation debug visualization
    if (pc.globalDataPtr.debugViewEquation > 0.0) {
        int index = int(pc.globalDataPtr.debugViewEquation);
        switch (index) {
            case 1: // Diffuse contribution
                color = diffuseContrib;
                break;
            case 2: // Fresnel (F)
                color = F;
                break;
            case 3: // Geometric occlusion (G)
                color = float3(G);
                break;
            case 4: // Microfacet distribution (D)
                color = float3(D);
                break;
            case 5: // Specular contribution
                color = specContrib;
                break;
        }
        color = linearToSRGB(color);
        return float4(color, baseColor.a);
    }

    // ========================================================================
    // Tone mapping and gamma correction
    // ========================================================================
    
    // Reinhard tone mapping
    color = color / (color + float3(1.0));
    
    // Gamma correction
    color = linearToSRGB(color);
    
    return float4(color, baseColor.a);
}





