struct CameraData
{
    float4x4 view;
    float4x4 projection;
};

[[vk::binding(0, 0)]]
ConstantBuffer<CameraData> camera;

[[vk::binding(0, 1)]]
SamplerCube sampler;

static const float2 vertices[3] =
{
    float2(-1.0, -1.0),
    float2( 3.0, -1.0),
    float2(-1.0,  3.0)
};

static const float2 uvs[3] = 
{
    float2(0.0, 0.0),
    float2(2.0, 0.0),
    float2(0.0, 2.0)
};


struct VertexOutput 
{
    float2 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[shader("vertex")]
VertexOutput vertMain(uint vertexId : SV_VertexID) 
{
    float2 vertex = vertices[vertexId];
    // float3 viewSpaceRotatedPosition = mul((float3x3)camera.view, vertex);
    // float4 clipSpacePosition = mul(camera.projection, float4(viewSpaceRotatedPosition, 1.0));

    VertexOutput output;
    output.position = vertices[vertexId];
    output.uv = uvs[vertexId];
    return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput inVert) : SV_Target
{
    return float4(1.0, 0.0, 0.0, 1.0);
}
