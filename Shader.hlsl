cbuffer ObjectCB : register(b0)
{
    float4x4 world;
    float4x4 worldViewProj;
    float3 lightPosition;
    float lightIntensity;
    float3 cameraPosition;
    float padding0;
    float4 baseColor;
};

// Dodajemy obsługę tekstury
Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

struct VSInput
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float2 uv : TEXCOORD1; // Przekazujemy UV
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    float4 worldPos = mul(float4(input.position, 1.0f), world);
    output.position = mul(float4(input.position, 1.0f), worldViewProj);
    output.worldPos = worldPos.xyz;
    output.uv = input.texcoord; // Przypisujemy UV
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    // Obliczanie normalnej (żeby było widać bryłę - Flat Shading)
    float3 dpdx = ddx(input.worldPos);
    float3 dpdy = ddy(input.worldPos);
    float3 normal = normalize(cross(dpdy, dpdx));

    float3 viewDir = normalize(cameraPosition - input.worldPos);
    if (dot(normal, viewDir) < 0.0f) normal = -normal;

    // POBIERANIE KOLORU Z TEKSTURY
    float4 texColor = gTexture.Sample(gSampler, input.uv);

    float3 toLight = lightPosition - input.worldPos;
    float dist = max(length(toLight), 0.0001f);
    float3 lightDir = toLight / dist;

    float ndotl = saturate(dot(normal, lightDir));
    float attenuation = 1.0f / (1.0f + dist * dist * 0.1f);
    float lighting = 0.15f + ndotl * lightIntensity * attenuation;

    // Łączymy kolor tekstury, bazowy i oświetlenie
    return float4(texColor.rgb * baseColor.rgb * lighting, 1.0f);
}