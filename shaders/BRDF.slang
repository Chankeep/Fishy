// BRDF.slang — Cook-Torrance BRDF functions for PBR rendering
// Based on Khronos glTF 2.0 PBR specification

import Common;

// ============================================================================
// PBR Info Structure
// ============================================================================

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
// BRDF Functions
// ============================================================================

// Basic Lambertian diffuse
// Reference: Lambert's Photometria https://archive.org/details/lambertsphotome00lambgoog
float3 diffuseBRDF(float3 diffuseColor) {
    return diffuseColor / M_PI;
}

// Fresnel reflectance term (F)
// Implementation of fresnel from [4], Equation 15
float3 specularReflection(PBRInfo pbrInputs) {
    return pbrInputs.reflectance0 + (pbrInputs.reflectance90 - pbrInputs.reflectance0) * 
           pow(clamp(1.0 - pbrInputs.VdotH, 0.0, 1.0), 5.0);
}

// Specular geometric attenuation (G)
// Based on [1] Equation 4, with modifications from [2]
float geometricOcclusion(PBRInfo pbrInputs) {
    float NdotL = pbrInputs.NdotL;
    float NdotV = pbrInputs.NdotV;
    float r = pbrInputs.alphaRoughness;

    float attenuationL = 2.0 * NdotL / (NdotL + sqrt(r * r + (1.0 - r * r) * (NdotL * NdotL)));
    float attenuationV = 2.0 * NdotV / (NdotV + sqrt(r * r + (1.0 - r * r) * (NdotV * NdotV)));
    return attenuationL * attenuationV;
}

// Microfacet distribution (D) — GGX/Trowbridge-Reitz
// Follows SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
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
// Multi-Light Evaluation
// ============================================================================

// Evaluate a single light's contribution using the Cook-Torrance BRDF.
// Supports directional (type 0), point (type 1), and spot (type 2) lights.
float3 evaluateLight(LightData light, float3 worldPos, float3 N, float3 V,
                     float3 diffuseColor, float3 specularColor,
                     float3 specR0, float specR90, float alphaRoughness, float NdotV) {
    float3 L;
    float attenuation = 1.0;

    uint lightType = uint(light.positionAndType.w);

    if (lightType == 0) {
        // Directional light: negate ray direction to get surface-to-light
        L = -normalize(light.directionAndRange.xyz);
    } else {
        // Point / Spot light: L points from fragment to light
        float3 toLight = light.positionAndType.xyz - worldPos;
        float dist = length(toLight);
        L = toLight / dist;

        // Smooth distance attenuation (UE4-style)
        float range = light.directionAndRange.w;
        if (range > 0.0) {
            float ratio = dist / range;
            attenuation = max(1.0 - pow(clamp(ratio, 0.0, 1.0), 4.0), 0.0);
            attenuation = attenuation * attenuation / (dist * dist + 1.0);
        } else {
            attenuation = 1.0 / (dist * dist + 1.0);
        }

        if (lightType == 2) {
            // Spot light: cone angle falloff
            // Compare -L (from light toward surface) against light forward direction
            float cosTheta = dot(-L, normalize(light.directionAndRange.xyz));
            float innerCos = light.spotAngles.x;
            float outerCos = light.spotAngles.y;
            attenuation *= clamp((cosTheta - outerCos) / (innerCos - outerCos), 0.0, 1.0);
        }
    }

    float3 H = normalize(L + V);
    float NdotL = clamp(dot(N, L), 0.001, 1.0);
    float NdotH = clamp(dot(N, H), 0.0, 1.0);
    float LdotH = clamp(dot(L, H), 0.0, 1.0);
    float VdotH = clamp(dot(V, H), 0.0, 1.0);

    // Build PBRInfo for this light direction
    PBRInfo pbrInputs;
    pbrInputs.NdotL = NdotL;
    pbrInputs.NdotV = NdotV;
    pbrInputs.NdotH = NdotH;
    pbrInputs.LdotH = LdotH;
    pbrInputs.VdotH = VdotH;
    pbrInputs.perceptualRoughness = sqrt(alphaRoughness);
    pbrInputs.metalness = 0.0;
    pbrInputs.reflectance0 = specR0;
    pbrInputs.reflectance90 = float3(specR90);
    pbrInputs.alphaRoughness = alphaRoughness;
    pbrInputs.diffuseColor = diffuseColor;
    pbrInputs.specularColor = specularColor;

    // Evaluate Cook-Torrance BRDF
    float3 F = specularReflection(pbrInputs);
    float G = geometricOcclusion(pbrInputs);
    float D = microfacetDistribution(pbrInputs);

    float3 diffuseContrib = (float3(1.0) - F) * diffuseBRDF(diffuseColor);
    float3 specContrib = F * G * D / (4.0 * NdotL * NdotV);

    float3 lightColor = light.colorAndIntensity.xyz * light.colorAndIntensity.w;

    return NdotL * attenuation * lightColor * (diffuseContrib + specContrib);
}

// Get light attenuation for clearcoat (no BRDF, just geometric attenuation)
float getLightAttenuation(LightData light, float3 worldPos) {
    uint lightType = uint(light.positionAndType.w);
    if (lightType == 0) return 1.0;

    float3 toLight = light.positionAndType.xyz - worldPos;
    float dist = length(toLight);
    float3 L = toLight / dist;
    float attenuation = 1.0;

    float range = light.directionAndRange.w;
    if (range > 0.0) {
        float ratio = dist / range;
        attenuation = max(1.0 - pow(clamp(ratio, 0.0, 1.0), 4.0), 0.0);
        attenuation = attenuation * attenuation / (dist * dist + 1.0);
    }

    if (lightType == 2) {
        float cosT = dot(-L, normalize(light.directionAndRange.xyz));
        attenuation *= clamp((cosT - light.spotAngles.y) / (light.spotAngles.x - light.spotAngles.y), 0.0, 1.0);
    }

    return attenuation;
}
