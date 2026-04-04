struct CameraData
{
    float4x4 view;
    float4x4 projection;
};

[[vk::binding(0, 0)]]
ConstantBuffer<CameraData> camera;

[[vk::binding(0, 1)]]
SamplerCube sampler;

static const float3 vertices[8] =
{
    float3(-1.0, -1.0, -1.0),
    float3( 1.0, -1.0, -1.0),
    float3( 1.0,  1.0, -1.0),
    float3(-1.0,  1.0, -1.0),
    float3(-1.0, -1.0,  1.0),
    float3( 1.0, -1.0,  1.0),
    float3( 1.0,  1.0,  1.0),
    float3(-1.0,  1.0,  1.0)
};

static const uint indices[36] =
{
    0, 2, 1, 0, 3, 2,
    4, 5, 6, 4, 6, 7,
    0, 7, 3, 0, 4, 7,
    1, 2, 6, 1, 6, 5,
    0, 1, 5, 0, 5, 4,
    3, 6, 2, 3, 7, 6
};

struct VertexOutput 
{
    float4 position : SV_Position;
    float3 direction : TEXCOORD0;
};

[shader("vertex")]
VertexOutput vertMain(uint vertexId : SV_VertexID) 
{
    float3 vertex = vertices[indices[vertexId]];
    float3 viewSpaceRotatedPosition = mul((float3x3)camera.view, vertex);
    float4 clipSpacePosition = mul(camera.projection, float4(viewSpaceRotatedPosition, 1.0));

    VertexOutput output;
    output.position = clipSpacePosition;
    output.direction = vertex.xyz;
    return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput inVert) : SV_Target
{
    return sampler.Sample(inVert.direction);
}
