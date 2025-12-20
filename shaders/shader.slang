struct VSInput {
    float3 inPosition;
    float3 inColor;
    float2 inTexCoord;
};

struct UniformBuffer {
    float4x4 model;
    float4x4 View;
    float4x4 proj;
}
ConstantBuffer<UniformBuffer> ubo;

[[vk::binding(1, 0)]] 
Sampler2D texSampler;

struct VSOutput {
    float4 pos : SV_Position;
    float3 color;
    float2 texCoord;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = mul(ubo.proj, mul(ubo.View, mul(ubo.model, float4(input.inPosition, 1.0))));
    output.color = input.inColor;
    output.texCoord = input.inTexCoord;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_Target
{
    return texSampler.Sample(vertIn.texCoord);
}