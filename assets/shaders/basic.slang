struct VertexInput
{
    float3 position;
    float3 normal;
    float2 uv;
};

struct PushConstants
{
    float4x4 model;
    float4x4 normalMatrix;
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
    uint hasDiffuseImage;
};

[[vk::binding(0, 0)]]
ConstantBuffer<CameraData> camera;

[[vk::binding(0, 1)]]
ConstantBuffer<MaterialData> material;

[[vk::binding(1, 1)]]
Sampler2D texture;

struct VertexOutput 
{
    float4 position : SV_Position;
    float3 normal   : TEXCOORD0;
    float2 uv       : TEXCOORD1;
};

[shader("vertex")]
VertexOutput vertMain(VertexInput vertexInput) 
{
    float4 positionWorldSpace = mul(pc.model, float4(vertexInput.position, 1.0));
    float4 positionViewSpace = mul(camera.view, positionWorldSpace);
    float4 positionClipSpace = mul(camera.projection, positionViewSpace);

    VertexOutput output;
    output.position = positionClipSpace;
    output.normal = mul(float3x3(pc.normalMatrix), vertexInput.normal);
    output.uv = vertexInput.uv;
    return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput inVert) : SV_Target
{
    float4 color;
    if(material.hasDiffuseImage == 1)
    {
        color = texture.Sample(inVert.uv);
    }
    else
    {
        color = material.diffuseColor;
    }

    float3 directionalLightDirection = normalize(float3(-1.0f, -1.0f, 0.0));
    float3 directionalLightColor = float3(0.8f, 0.8f, 0.8f);
    float NdotL = max(dot(normalize(inVert.normal), -directionalLightDirection), 0.0);

    float3 ambientLight = color.rgb * 0.1f;
    float3 diffuseLight = color.rgb * directionalLightColor * NdotL;

    float3 finalColor = ambientLight + diffuseLight;

    return float4(finalColor, 1.0f); 
}
