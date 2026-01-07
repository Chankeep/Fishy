// Skybox Shader
// Renders a cubemap as the scene background

// ============================================================================
// Vertex Shader Input/Output
// ============================================================================

struct VSInput {
    [[vk::location(0)]] float3 inPosition;
    [[vk::location(1)]] float3 inNormal;    // 未使用，但 Vertex 结构需要
    [[vk::location(2)]] float2 inTexCoord;  // 未使用
    [[vk::location(3)]] float4 inTangent;   // 未使用
};

struct VSOutput {
    float4 pos : SV_Position;
    float3 localPos; // 用于采样 Cubemap 的方向向量
};

// ============================================================================
// Uniforms - 复用 Set 0 的 GlobalUBO
// ============================================================================

struct GlobalUBO {
    float4x4 view;
    float4x4 proj;
    float3 camPos;
    float3 lightDir;
    float3 lightColor;
    float debugViewInputs;
    float debugViewEquation;
    float prefilteredMipLevels;
    float iblIntensity;
};
[[vk::binding(0, 0)]] ConstantBuffer<GlobalUBO> ubo;

// IBL 预过滤环境贴图 (复用 Set 0, binding 2)
[[vk::binding(2, 0)]] SamplerCube prefilteredEnvMap;

// ============================================================================
// Vertex Shader
// ============================================================================

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    
    // 传递本地位置作为 Cubemap 采样方向
    output.localPos = input.inPosition;
    
    // 移除 View 矩阵的平移分量，只保留旋转
    // 这样 Skybox 只随相机旋转而变化，不随平移而变化
    float4x4 rotView = ubo.view;
    rotView[0][3] = 0.0;
    rotView[1][3] = 0.0;
    rotView[2][3] = 0.0;
    
    float4 clipPos = mul(ubo.proj, mul(rotView, float4(input.inPosition, 1.0)));
    
    // 将 z 设为 w，确保深度始终为 1.0 (最远处)
    // NDC 深度 = z/w = w/w = 1.0
    output.pos = clipPos.xyww;
    
    return output;
}

// ============================================================================
// Fragment Shader
// ============================================================================

[shader("fragment")]
float4 fragMain(VSOutput input) : SV_Target {
    // 使用本地位置作为方向向量采样环境贴图
    // 使用 LOD 0 获取最高分辨率的环境贴图
    float3 envColor = prefilteredEnvMap.SampleLevel(normalize(input.localPos), 0.0).rgb;
    
    // Tone mapping (Reinhard)
    envColor = envColor / (envColor + float3(1.0));
    
    // Gamma correction
    envColor = pow(envColor, float3(1.0 / 2.2));
    
    return float4(envColor, 1.0);
}