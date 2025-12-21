struct VSInput {
    float3 inPosition;
    float3 inNormal;
    float2 inTexCoord;
    float3 inTangent;
};

// Set 0: Global Data
struct UniformBuffer {
    float4x4 model;
    float4x4 View;
    float4x4 proj;
    float3 camPos;
    float3 lightDir;
    float3 lightColor;
}
[[vk::binding(0, 0)]] ConstantBuffer<UniformBuffer> ubo;

// Set 1: Material Data
[[vk::binding(0, 1)]] Sampler2D albedoMap;
[[vk::binding(1, 1)]] Sampler2D metallicMap;
[[vk::binding(2, 1)]] Sampler2D roughnessMap;
[[vk::binding(3, 1)]] Sampler2D normalMap;
[[vk::binding(4, 1)]] Sampler2D aoMap;
[[vk::binding(5, 1)]] Sampler2D heightMap;

struct VSOutput {
    float4 pos : SV_Position;
    float3 worldPos;
    float3 color;
    float2 texCoord;
    float3 worldNormal;
    float3 worldTangent;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    float4 worldPos = mul(ubo.model, float4(input.inPosition, 1.0));
    output.worldPos = worldPos.xyz;
    output.pos = mul(ubo.proj, mul(ubo.View, worldPos));
    output.color = float3(1.0, 1.0, 1.0); // Default white color for models
    output.texCoord = input.inTexCoord;
    
    // Transform normal and tangent to world space
    // Assuming uniform scaling (otherwise use inverse-transpose for normal)
    output.worldNormal = normalize(mul((float3x3)ubo.model, input.inNormal));
    output.worldTangent = normalize(mul((float3x3)ubo.model, input.inTangent));

    return output;
}

static const float PI = 3.14159265359;

float DistributionGGX(float3 N, float3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, 0.0000001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

float3 FresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float2 ParallaxMapping(float2 texCoords, float3 viewDir) {
    // Number of depth layers
    const float minLayers = 8;
    const float maxLayers = 32;
    float numLayers = lerp(maxLayers, minLayers, abs(dot(float3(0, 0, 1), viewDir)));  
    
    // Calculate the size of each layer
    float layerDepth = 1.0 / numLayers;
    // Depth of current layer
    float currentLayerDepth = 0.0;
    
    // Amount to shift the UV coordinates per layer
    // 0.1 is the height scale
    float heightScale = 0.05; 
    float2 P = viewDir.xy / viewDir.z * heightScale; 
    float2 deltaTexCoords = P / numLayers;
    
    // Get initial values
    float2  currentTexCoords = texCoords;
    float currentDepthMapValue = 1.0 - heightMap.Sample(currentTexCoords).r; // Inverse for consistency if needed
    
    while(currentLayerDepth < currentDepthMapValue) {
        // Shift texture coordinates along direction of P
        currentTexCoords -= deltaTexCoords;
        // Get depthmap value at current texture coordinates
        currentDepthMapValue = 1.0 - heightMap.Sample(currentTexCoords).r;  
        // Get depth of next layer
        currentLayerDepth += layerDepth;  
    }
    
    // Parallax Occlusion Mapping (smoothing)
    float2 prevTexCoords = currentTexCoords + deltaTexCoords;
    float afterDepth  = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = (1.0 - heightMap.Sample(prevTexCoords).r) - (currentLayerDepth - layerDepth);
    
    float weight = afterDepth / (afterDepth - beforeDepth);
    float2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

    return finalTexCoords;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_Target
{
    float3 N = normalize(vertIn.worldNormal);
    float3 T = normalize(vertIn.worldTangent);
    // Re-orthogonalize T with respect to N
    T = normalize(T - dot(T, N) * N);
    float3 B = cross(N, T);
    float3x3 TBN = float3x3(T, B, N);

    float3 viewDir = normalize(ubo.camPos - vertIn.worldPos);
    
    // Transform ViewDir to Tangent Space for Parallax Mapping
    float3 tangentViewDir = normalize(mul(TBN, viewDir)); // mul(vector, matrix) is row-vector mult, effectively V * Transpose(M) if M is orthonormal
    // Wait, TBN is column major: [T B N]. mul(v, M) = v.x*T + v.y*B + v.z*N. This transforms World -> Tangent. Correct.

    // Apply Parallax Mapping
    float2 texCoord = vertIn.texCoord;
    texCoord = ParallaxMapping(texCoord, tangentViewDir);

    // Discard fragments outside 0-1 range to avoid wrapping artifacts at edges
    if(texCoord.x > 1.0 || texCoord.y > 1.0 || texCoord.x < 0.0 || texCoord.y < 0.0)
         discard;

    float3 albedo = pow(albedoMap.Sample(texCoord).rgb, float3(2.2)); // Gamma to Linear
    float metallic = metallicMap.Sample(texCoord).r;
    float roughness = roughnessMap.Sample(texCoord).r;
    float ao = aoMap.Sample(texCoord).r;
    
    // Normal Mapping
    // Use the NEW texCoords
    float3 tangentNormal = normalMap.Sample(texCoord).rgb * 2.0 - 1.0;
    
    // Re-calculate TBN or transform tangentNormal to world
    N = normalize(mul(tangentNormal, TBN)); 

    float3 V = viewDir;

    // Reflectance at normal incidence
    float3 F0 = float3(0.04); 
    F0 = lerp(F0, albedo, metallic);

    // Light calculation
    float3 L = normalize(ubo.lightDir);
    float3 H = normalize(V + L);
    float3 radiance = ubo.lightColor; 

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);   
    float G = GeometrySmith(N, V, L, roughness);      
    float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    
    float3 numerator = NDF * G * F; 
    float denom = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; 
    float3 specular = numerator / denom;
    
    float3 kS = F;
    float3 kD = float3(1.0) - kS;
    kD *= 1.0 - metallic;	  

    float NdotL = max(dot(N, L), 0.0);        

    // Outgoing radiance Lo
    float3 Lo = (kD * albedo / PI + specular) * radiance * NdotL;
    
    // Ambient
    float3 ambient = float3(0.03) * albedo * ao;
    
    float3 color = ambient + Lo;

    // HDR Tone mapping (Reinhard)
    color = color / (color + float3(1.0));
    // Gamma correct
    color = pow(color, float3(1.0/2.2)); 

    return float4(color, 1.0);
}