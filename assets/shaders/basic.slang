struct VertexInput
{
    float2 position;
    float3 color;
};

struct UniformBuffer 
{
    float4x4 model;
    float4x4 view;
    float4x4 projection;
}
ConstantBuffer<UniformBuffer> ubo;

struct VertexOutput 
{
    float3 color;
    float4 position : SV_Position;
};

[shader("vertex")]
VertexOutput vertMain(VertexInput vertexInput) 
{
    VertexOutput output;
    output.position = mul(ubo.projection, mul(ubo.view, mul(ubo.model, float4(vertexInput.position, 0.0, 1.0))));
    output.color = vertexInput.color;
    return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput inVert) : SV_Target
{
    float3 color = inVert.color;
    return float4(color, 1.0);
}