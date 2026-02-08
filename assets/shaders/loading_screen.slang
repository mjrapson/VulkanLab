[[vk::binding(0, 0)]]
Sampler2D sampler;

static const float2 vertices[4] =
{
    float2(-1.0, -1.0),
    float2( 1.0, -1.0),
    float2( 1.0,  1.0),
    float2(-1.0,  1.0),
};

static const uint indices[6] =
{
    0, 1, 2, 2, 3, 0,
};

struct VertexOutput 
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[shader("vertex")]
VertexOutput vertMain(uint vertexId : SV_VertexID) 
{
    float4 vertexClipSpace = float4(vertices[indices[vertexId]], 0.0, 1.0);

    float uvx = 0.5 * vertexClipSpace.x + 0.5;
    float uvy = 1.0 - (0.5 * vertexClipSpace.y + 0.5);

    VertexOutput output;
    output.position = vertexClipSpace;
    output.uv = float2(uvx, uvy);
    return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput inVert) : SV_Target
{
  return sampler.Sample(inVert.uv);
}
