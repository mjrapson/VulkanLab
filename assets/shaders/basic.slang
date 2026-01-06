struct VertexInput
{
    float3 position;
    float3 normal;
    float2 uv;
};

struct PushConstants
{
    float4x4 model;
};

[[vk::push_constant]]
PushConstants pc;

struct CameraData
{
    float4x4 view;
    float4x4 projection;
};

struct MaterialData
{
    float4 diffuseColor;
};

[[vk::binding(0, 0)]]
ConstantBuffer<CameraData> camera;

[[vk::binding(0, 1)]]
ConstantBuffer<MaterialData> material;

[[vk::binding(1, 1)]]
Sampler2D textures[];

struct VertexOutput 
{
    float4 position : SV_Position;
    float3 normal   : TEXCOORD0;
    float2 uv       : TEXCOORD1;
};

[shader("vertex")]
VertexOutput vertMain(VertexInput vertexInput) 
{
    VertexOutput output;
    output.position = mul(camera.projection, mul(camera.view, mul(pc.model, float4(vertexInput.position, 1.0))));
    output.normal = vertexInput.normal;
    output.uv = vertexInput.uv;
    return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput inVert) : SV_Target
{
    return float4(0.4, 0.8, 0.8, 1.0);
}
