struct VertexInput
{
    float3 position;
};

struct PushConstants
{
    float4x4 model;
};

[[vk::push_constant]]
PushConstants pc;

struct DirectionalLightData
{
    float3 direction;
    uint _padding1;
    float4x4 lightSpaceView;
    float4x4 lightSpaceProjection;
}

[[vk::binding(0, 0)]]
ConstantBuffer<DirectionalLightData> directionalLight;

struct VertexOutput 
{
    float4 position : SV_Position;
};

[shader("vertex")]
VertexOutput vertMain(VertexInput vertexInput) 
{
    float4 positionWorldSpace = mul(pc.model, float4(vertexInput.position, 1.0));
    float4 positionLightViewSpace = mul(directionalLight.lightSpaceView, positionWorldSpace);
    float4 positionLightClipSpace = mul(directionalLight.lightSpaceProjection, positionLightViewSpace);

    VertexOutput output;
    output.position = positionLightClipSpace;
    return output;
}

[shader("fragment")]
void fragMain()
{
}
