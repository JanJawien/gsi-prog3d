struct Light
{
    float3 position;
    float intensity;
    float3 color;
    float range;        
    float3 direction;   
    float spotPower;    
    int type;           
    int isEnabled;
    float2 padding;
};

cbuffer ObjectCB : register(b0)
{
    float4x4 world;
    float4x4 worldViewProj;
    float3 cameraPosition;
    float uvScale;
    float4 baseColor;

    Light lights[16];
    int lightCount;
    float3 padding2;
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
    float2 tiledUV = input.uv.yx * uvScale;
    float4 texColor = gTexture.Sample(gSampler, tiledUV);

    float3 dpdx = ddx(input.worldPos);
    float3 dpdy = ddy(input.worldPos);
    float3 normal = normalize(cross(dpdy, dpdx));

    float3 viewDir = normalize(cameraPosition - input.worldPos);
    if (dot(normal, viewDir) < 0.0f) normal = -normal;

    float3 totalLight = float3(0, 0, 0);

    [loop]
    for (int i = 0; i < lightCount; i++)
    {
        Light L = lights[i];
        if (L.isEnabled == 0)
            continue;

        float3 toLight = L.position - input.worldPos;
        float dist = length(toLight);
        float3 Ldir = toLight / max(dist, 0.0001f);

        float ndotl = saturate(dot(normal, Ldir));

        // attenuation
        float att = saturate(1.0f - dist / L.range);
        att *= att;

        float spot = 1.0f;

        // spotlight
        if (L.type == 1)
        {
            float3 sd = normalize(L.direction);
            float theta = dot(-Ldir, sd);
            spot = pow(saturate(theta), L.spotPower);
        }

        float3 lightColor = L.color * L.intensity * ndotl * att * spot;

        totalLight += lightColor;
    }

    float3 ambient = float3(0.15f, 0.15f, 0.15f);

    float3 finalColor = texColor.rgb * baseColor.rgb * (ambient + totalLight);

    return float4(finalColor, 1.0f);
}