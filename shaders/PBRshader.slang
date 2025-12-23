// glTF PBR Shader with Extensions
// Based on Khronos glTF 2.0 PBR specification and WebGL PBR implementation
// See https://github.com/KhronosGroup/glTF-WebGL-PBR
// Supports: KHR_materials_clearcoat, KHR_materials_ior, KHR_materials_emissive_strength

struct VSInput {
    float3 inPosition;
    float3 inNormal;
    float2 inTexCoord;
    float4 inTangent; // xyz = tangent, w = handedness
};

// Set 0: Global Data
struct GlobalUBO {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float3 camPos;
    float3 lightDir;
    float3 lightColor;
};
[[vk::binding(0, 0)]] ConstantBuffer<GlobalUBO> ubo;

// Set 1: Material Data
struct MaterialUBO {
    float4 baseColorFactor;           // offset 0
    float metallicFactor;             // offset 16
    float roughnessFactor;            // offset 20
    float normalScale;                // offset 24
    float occlusionStrength;          // offset 28
    float4 emissiveFactor;            // offset 32, w = emissiveStrength
    float alphaCutoff;                // offset 48
    uint flags;                       // offset 52
    float clearcoatFactor;            // offset 56
    float clearcoatRoughnessFactor;   // offset 60
    float transmissionFactor;         // offset 64
    float ior;                        // offset 68
    float3 padding;                   // offset 72
};
[[vk::binding(0, 1)]] ConstantBuffer<MaterialUBO> material;

// Material textures
[[vk::binding(1, 1)]] Sampler2D baseColorMap;
[[vk::binding(2, 1)]] Sampler2D metallicRoughnessMap;
[[vk::binding(3, 1)]] Sampler2D normalMap;
[[vk::binding(4, 1)]] Sampler2D occlusionMap;
[[vk::binding(5, 1)]] Sampler2D emissiveMap;
[[vk::binding(6, 1)]] Sampler2D clearcoatMap;
[[vk::binding(7, 1)]] Sampler2D clearcoatRoughnessMap;
[[vk::binding(8, 1)]] Sampler2D clearcoatNormalMap;
[[vk::binding(9, 1)]] Sampler2D transmissionMap;

struct VSOutput {
    float4 pos : SV_Position;
    float3 worldPos;
    float2 texCoord;
    float3 worldNormal;
    float3 worldTangent;
    float tangentW;
};

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

// Find the normal for this fragment, pulling either from a predefined normal map
// or from the interpolated mesh normal and tangent attributes.
// Reference: http://www.thetenthplanet.de/archives/1180
float3 getNormal(VSOutput vertIn) {
    float3 tangentNormal = normalMap.Sample(vertIn.texCoord).rgb * 2.0 - 1.0;
    tangentNormal.xy *= material.normalScale;
    tangentNormal = normalize(tangentNormal);
    
    float3 N = normalize(vertIn.worldNormal);
    
    // Check if we have valid tangent data (tangentW != 0 indicates valid tangent)
    if (abs(vertIn.tangentW) > 0.001) {
        // Use provided tangent
        float3 T = normalize(vertIn.worldTangent);
        T = normalize(T - dot(T, N) * N); // Gram-Schmidt re-orthogonalization
        float3 B = cross(N, T) * vertIn.tangentW;
        float3x3 TBN = float3x3(T, B, N);
        return normalize(mul(tangentNormal, TBN));
    } else {
        // Perturb normal using screen-space derivatives (no tangent provided)
        float3 q1 = ddx(vertIn.worldPos);
        float3 q2 = ddy(vertIn.worldPos);
        float2 st1 = ddx(vertIn.texCoord);
        float2 st2 = ddy(vertIn.texCoord);
        
        float3 T = normalize(q1 * st2.y - q2 * st1.y);
        float3 B = -normalize(cross(N, T));
        float3x3 TBN = float3x3(T, B, N);
        
        return normalize(mul(tangentNormal, TBN));
    }
}

// Get clearcoat normal (separate from base normal)
float3 getClearcoatNormal(VSOutput vertIn, float3 geometricNormal) {
    // Check if clearcoat normal texture is present (bit 10)
    if ((material.flags & (1 << 10)) == 0) {
        return geometricNormal;
    }
    
    float3 tangentNormal = clearcoatNormalMap.Sample(vertIn.texCoord).rgb * 2.0 - 1.0;
    float3 N = normalize(vertIn.worldNormal);
    
    if (abs(vertIn.tangentW) > 0.001) {
        float3 T = normalize(vertIn.worldTangent);
        T = normalize(T - dot(T, N) * N);
        float3 B = cross(N, T) * vertIn.tangentW;
        float3x3 TBN = float3x3(T, B, N);
        return normalize(mul(tangentNormal, TBN));
    } else {
        float3 q1 = ddx(vertIn.worldPos);
        float3 q2 = ddy(vertIn.worldPos);
        float2 st1 = ddx(vertIn.texCoord);
        float2 st2 = ddy(vertIn.texCoord);
        
        float3 T = normalize(q1 * st2.y - q2 * st1.y);
        float3 B = -normalize(cross(N, T));
        float3x3 TBN = float3x3(T, B, N);
        
        return normalize(mul(tangentNormal, TBN));
    }
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
VSOutput vertMain(VSInput input) {
    VSOutput output;
    float4 worldPos = mul(ubo.model, float4(input.inPosition, 1.0));
    output.worldPos = worldPos.xyz;
    output.pos = mul(ubo.proj, mul(ubo.view, worldPos));
    output.texCoord = input.inTexCoord;
    
    // Transform normal and tangent to world space
    float3x3 normalMatrix = (float3x3)ubo.model;
    output.worldNormal = normalize(mul(normalMatrix, input.inNormal));
    output.worldTangent = normalize(mul(normalMatrix, input.inTangent.xyz));
    output.tangentW = input.inTangent.w;

    return output;
}

// ============================================================================
// Fragment Shader
// ============================================================================

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_Target
{
    float2 uv = vertIn.texCoord;
    
    // ========================================================================
    // Sample textures and apply material factors
    // ========================================================================
    
    float4 baseColor = sRGBToLinear(baseColorMap.Sample(uv)) * material.baseColorFactor;
    
    // Alpha handling
    uint alphaMode = (material.flags >> 1) & 0x3;
    if (alphaMode == 1) { // MASK mode
        if (baseColor.a < material.alphaCutoff) {
            discard;
        }
        baseColor.a = 1.0;
    }
    
    // glTF: metallic in B channel, roughness in G channel
    float4 mrSample = metallicRoughnessMap.Sample(uv);
    float perceptualRoughness = clamp(mrSample.g * material.roughnessFactor, MIN_ROUGHNESS, 1.0);
    float metallic = clamp(mrSample.b * material.metallicFactor, 0.0, 1.0);
    
    // Roughness is authored as perceptual roughness; as is convention,
    // convert to material roughness by squaring the perceptual roughness
    float alphaRoughness = perceptualRoughness * perceptualRoughness;
    
    // ========================================================================
    // Calculate reflectance
    // ========================================================================
    
    // Calculate F0 based on IOR (default 1.5 gives 0.04)
    float3 f0 = F0FromIOR(material.ior);
    
    float3 diffuseColor = baseColor.rgb * (float3(1.0) - f0) * (1.0 - metallic);
    float3 specularColor = lerp(f0, baseColor.rgb, metallic);
    
    // Compute reflectance
    float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);
    
    // For typical incident reflectance range (between 4% to 100%) set the grazing reflectance to 100% for typical fresnel effect.
    // For very low reflectance range on highly diffuse objects (below 4%), incrementally reduce grazing reflectance to 0%.
    float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
    float3 specularEnvironmentR0 = specularColor;
    float3 specularEnvironmentR90 = float3(reflectance90);
    
    // ========================================================================
    // Get vectors for lighting calculation
    // ========================================================================
    
    // Check if normal map is present (bit 3)
    float3 n = ((material.flags & (1 << 3)) != 0) ? getNormal(vertIn) : normalize(vertIn.worldNormal);
    float3 v = normalize(ubo.camPos - vertIn.worldPos);    // Vector from surface point to camera
    float3 l = normalize(ubo.lightDir);                    // Vector from surface point to light
    float3 h = normalize(l + v);                           // Half vector between both l and v
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
    float3 color = NdotL * ubo.lightColor * (diffuseContrib + specContrib);
    
    // ========================================================================
    // Clearcoat layer (KHR_materials_clearcoat)
    // ========================================================================
    
    float clearcoatFactor = material.clearcoatFactor;
    if (clearcoatFactor > 0.0) {
        float clearcoatStrength = clearcoatMap.Sample(uv).r * clearcoatFactor;
        float clearcoatRoughness = clamp(clearcoatRoughnessMap.Sample(uv).g * material.clearcoatRoughnessFactor, MIN_ROUGHNESS, 1.0);
        
        float3 clearcoatN = getClearcoatNormal(vertIn, normalize(vertIn.worldNormal));
        float3 clearcoatBRDF = evaluateClearcoatBRDF(clearcoatN, v, l, clearcoatRoughness) * ubo.lightColor;
        
        // Fresnel for clearcoat energy conservation
        float clearcoatNdotV = max(dot(clearcoatN, v), 0.0);
        float clearcoatFresnel = 0.04 + (1.0 - 0.04) * pow(1.0 - clearcoatNdotV, 5.0);
        
        // Blend: clearcoat on top, attenuating base layer
        color = color * (1.0 - clearcoatStrength * clearcoatFresnel) + clearcoatBRDF * clearcoatStrength;
    }
    
    // ========================================================================
    // Apply ambient occlusion
    // ========================================================================
    
    // Check if occlusion map is present (bit 4)
    if ((material.flags & (1 << 4)) != 0) {
        float ao = occlusionMap.Sample(uv).r;
        color = lerp(color, color * ao, material.occlusionStrength);
    }
    
    // ========================================================================
    // Ambient lighting (simple approximation without IBL)
    // ========================================================================
    
    float3 ambient = float3(0.03) * baseColor.rgb;
    color += ambient;
    
    // ========================================================================
    // Emissive
    // ========================================================================
    
    // Check if emissive map is present (bit 5)
    float emissiveStrength = material.emissiveFactor.w;
    float3 emissive = material.emissiveFactor.rgb * emissiveStrength;
    if ((material.flags & (1 << 5)) != 0) {
        emissive *= sRGBToLinear(emissiveMap.Sample(uv)).rgb;
    }
    color += emissive;
    
    // ========================================================================
    // Tone mapping and gamma correction
    // ========================================================================
    
    // Reinhard tone mapping
    color = color / (color + float3(1.0));
    
    // Gamma correction
    color = linearToSRGB(color);
    
    return float4(color, baseColor.a);
}