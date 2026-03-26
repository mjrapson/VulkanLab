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

struct DirectionalLightData
{
    float3 direction;
    uint _padding1;
    float4x4 lightSpaceView;
    float4x4 lightSpaceProjection;
}

[[vk::binding(0, 0)]]
ConstantBuffer<CameraData> camera;

[[vk::binding(0, 1)]]
ConstantBuffer<MaterialData> material;

[[vk::binding(1, 1)]]
Sampler2D texture;

[[vk::binding(0, 2)]]
ConstantBuffer<DirectionalLightData> directionalLight;

[[vk::binding(0, 3)]]
Sampler2D shadowMapTexture;

struct VertexOutput 
{
    float4 position : SV_Position;
    float3 normal   : TEXCOORD0;
    float2 uv       : TEXCOORD1;
    float4 lightSpacePosition;
};

//////////
// Shadow mapping effectively does the following steps:
// 1. Map the fragment from its NDC light space to UV map coordinates [-1, 1]
// If the UV coordinates are out of bounds, assume no shadow
//
// 2. Get the depth value (Z) for the fragment from its light space projection
// For Vulkan, its already within the range 0 - 1
//
// 3.Compare the fragment depth (from the light POV) to the depth of the closest
// fragment in the shadow map to determine if the fragment is shadowed by an object
// closer to the light that it or not.
//
// This is done using SampleCmp with the depth comparison function defined in the pipeline
// (compareOp) which gives us hardware PCF (i.e. averaging the comparison with neighbouring fragments
// to produce software shadows).
// 
// The result of the comparison (the average) is between 0.0 and 1.0, i.e. fully shadowed (0.0) to
// not shadowed (1.0). The inverse of this value gives us a factor to apply to our light, to 
// reduce it, for example, a sample result of 0.7 gives an inverse factor 1.0 - 0.7 = 0.3, or 
// in other words, 70% light reduction.
//
// The comparison also uses a small sloped-bias to reduce shadow acne due to precision loss
// on the shadow map and in calculations here
//////////
float calculateShadowFactor(float4 fragmentLightSpacePosition, float NdotL)
{
    float3 shadowCoord = fragmentLightSpacePosition.xyz / fragmentLightSpacePosition.w;

    float2 shadowUV = shadowCoord.xy * 0.5 + 0.5;
    if (shadowUV.x < 0.0 || shadowUV.x > 1.0 || shadowUV.y < 0.0 || shadowUV.y > 1.0) 
    {
        return 1.0f;
    }

    float currentDepth = shadowCoord.z;
    float bias = max(0.005 * (1.0 - NdotL), 0.0005);

    return (1.0f - shadowMapTexture.SampleCmp(shadowUV, currentDepth - bias));
}

[shader("vertex")]
VertexOutput vertMain(VertexInput vertexInput) 
{
    float4 positionWorldSpace = mul(pc.model, float4(vertexInput.position, 1.0));
    float4 positionViewSpace = mul(camera.view, positionWorldSpace);
    float4 positionClipSpace = mul(camera.projection, positionViewSpace);

    VertexOutput output;
    output.position = positionClipSpace;
    output.normal = normalize(mul(float3x3(pc.normalMatrix), vertexInput.normal));
    output.uv = vertexInput.uv;
    output.lightSpacePosition = mul(directionalLight.lightSpaceProjection, mul(directionalLight.lightSpaceView, mul(pc.model, float4(vertexInput.position, 1.0))));
    return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput inVert) : SV_Target
{
    // Fetch the base colour of the fragment
    float4 color;
    if(material.hasDiffuseImage == 1)
    {
        color = texture.Sample(inVert.uv);
    }
    else
    {
        color = material.diffuseColor;
    }

    // Get the alignment between the light and the surface normal
    float3 directionalLightDirection = normalize(-directionalLight.direction);
    float3 directionalLightColor = float3(0.8f, 0.8f, 0.8f);
    float NdotL = max(dot(normalize(inVert.normal), directionalLightDirection), 0.0);

    // Get how much the fragment is shadowed
    float shadowCoefficient = calculateShadowFactor(inVert.lightSpacePosition, NdotL);

    // Get raw light values
    float3 ambientLight = color.rgb * 0.1f;
    float3 diffuseLight = color.rgb * directionalLightColor * NdotL;

    // Sum lighting and apply shadow effects
    float3 finalColor = ambientLight + (shadowCoefficient * diffuseLight);

    return float4(finalColor, 1.0f);
}
