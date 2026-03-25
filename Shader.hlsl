
cbuffer ObjectCB : register(b0)
{
    float4x4 worldViewProj;
};

Texture2D albedoTexture : register(t0);
SamplerState linearWrapSampler : register(s0);

struct VSInput
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.position = mul(float4(input.position, 1.0f), worldViewProj);
    output.texcoord = input.texcoord;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return albedoTexture.Sample(linearWrapSampler, input.texcoord);
}
