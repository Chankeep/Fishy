
struct VSInput {
    float2 inPosition;
    float3 inColor;
};

struct UniformBuffer {
    float4x4 model;
    float4x4 View;
    float4x4 proj;
}
ConstantBuffer<UniformBuffer> ubo;

struct VSOutput {
    float4 pos : SV_Position;
    float3 color;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = mul(ubo.proj, mul(ubo.View, mul(ubo.model, float4(input.inPosition, 0.0, 1.0))));
    output.color = input.inColor;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_Target
{
    return float4(vertIn.color, 1.0);
}