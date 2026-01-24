struct CameraData
{
    float4x4 view;
    float4x4 projection;
};

[[vk::binding(0, 0)]]
ConstantBuffer<CameraData> camera;

// [[vk::binding(1, 1)]]
// SamplerState sampler;
// 
// [[vk::binding(1, 2)]]
// TextureCube texture;

struct VertexOutput 
{
    float4 position : SV_Position;
    float2 ndc: TEXCOORD0;
};

[shader("vertex")]
VertexOutput vertMain(uint vertexId : SV_VertexID) 
{
    float2 vertexPositions[3] = {
        float2(-1.0, -1.0),
        float2(3.0, -1.0),
        float2(-1.0, 3.0)
    };

    VertexOutput output;
    output.position = float4(vertexPositions[vertexId], .0, 1.0);
    output.ndc = vertexPositions[vertexId];
    return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput inVert) : SV_Target
{
    return float4(0.4, 0.8, 0.4, 1.0);
}
