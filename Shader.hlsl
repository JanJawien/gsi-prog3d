struct Light
{
    float3 position;
    float intensity; // light brightness
    float3 color;
    float range; // light range
    float3 direction; // light direction, important for spotlights
    float spotPower; // spotlight cone sharpness
    int type; // 0 = ambient/general, 1 = spotlight
    int isEnabled; // 0 = disabled, 1 = enabled
    float2 padding;
};

cbuffer ObjectCB : register(b0)
{
    float4x4 world;
    float4x4 worldViewProj;
    float3 cameraPosition;
    float uvScale; // texture tiling/repetition
    float4 baseColor; // base object color

    // must match MAX_LIGHTS in StructDef.h
    Light lights[16];

    // current number of lights
    int lightCount;

    // 1 when the rendered object is a visible light cone
    int isLightCone;
    float2 padding;
};

// object texture
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
    float2 uv : TEXCOORD1;
};

float3 ComputeSpotlightCone(
    Light L,
    float3 worldPos)
{
    // calculates visible spotlight cone/glow
    float3 sd = normalize(L.direction);

    float3 fromLight = worldPos - L.position;

    // distance along spotlight axis
    float axialDist = dot(fromLight, sd);

    // only in front of light and inside range
    if (axialDist <= 0.0f || axialDist >= L.range)
        return float3(0, 0, 0);

    // closest point on spotlight axis
    float3 projected = L.position + sd * axialDist;

    // radial distance from cone axis
    float radialDist = length(worldPos - projected);

    // spotlight cone width, higher value = wider cone
    float coneRadius = axialDist * 0.35f;

    // visible cone geometry is widened to make the effect clearer
    if (isLightCone != 0)
        coneRadius *= 2.5f;

    // soft cone edge
    float coneMask =
        1.0f - smoothstep(
            coneRadius * 0.7f,
            coneRadius,
            radialDist);

    // distance fade
    float distanceFade =
        1.0f - (axialDist / L.range);

    distanceFade *= distanceFade;

    // brighter center
    float centerGlow =
        1.0f - saturate(radialDist / coneRadius);

    centerGlow = pow(centerGlow, 2.0f);

    // final visible cone strength, 0.55f changes glow brightness
    float coneIntensity =
        coneMask *
        centerGlow *
        distanceFade *
        L.intensity *
        0.55f;

    return L.color * coneIntensity;
}

PSInput VSMain(VSInput input)
{
    PSInput output;
    float4 worldPos = mul(float4(input.position, 1.0f), world);
    output.position = mul(float4(input.position, 1.0f), worldViewProj);
    output.worldPos = worldPos.xyz;
    output.uv = input.texcoord;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    // uvScale changes texture repetition on the object
    float2 tiledUV = input.uv.yx * uvScale;
    float4 texColor = gTexture.Sample(gSampler, tiledUV);

    // normal calculated from world position
    float3 dpdx = ddx(input.worldPos);
    float3 dpdy = ddy(input.worldPos);
    float3 normal = normalize(cross(dpdy, dpdx));

    float3 viewDir = normalize(cameraPosition - input.worldPos);
    if (dot(normal, viewDir) < 0.0f)
        normal = -normal;

    float3 totalLight = float3(0, 0, 0);

    // loop through all lights
    [loop]
    for (int i = 0; i < lightCount; i++)
    {
        Light L = lights[i];

        // skip disabled lights
        if (L.isEnabled == 0)
            continue;

        // when rendering light cones, skip ambient/general light
        if (isLightCone != 0 && L.type == 0)
            continue;

        float3 toLight = L.position - input.worldPos;
        float dist = length(toLight);
        float3 Ldir = toLight / max(dist, 0.0001f);

        float ndotl = saturate(dot(normal, Ldir));

        // attenuation = light fades with distance
        float att = saturate(1.0f - dist / L.range);
        att *= att;

        float spot = 1.0f;

        // spotlight / directional reflector
        if (L.type == 1)
        {
            float3 sd = normalize(L.direction);
            float theta = dot(-Ldir, sd);

            // spotPower changes cone sharpness
            spot = pow(saturate(theta), L.spotPower);

            // add visible cone glow
            totalLight += ComputeSpotlightCone(L, input.worldPos);
        }

        float3 lightColor = L.color * L.intensity * ndotl * att * spot;

        totalLight += lightColor;
    }

    // base scene light, higher values = brighter scene without spotlights
    float3 ambient = float3(0.15f, 0.15f, 0.15f);

    float3 finalColor = texColor.rgb * baseColor.rgb * (ambient + totalLight);
    float finalAlpha = texColor.a;

    // special rendering for transparent light cone
    if (isLightCone != 0)
    {
        finalColor = totalLight;

        // light cone transparency, final *2.0f boosts the effect
        finalAlpha *= (finalColor.x + finalColor.y + finalColor.z) / 3.0f * saturate(dot(normal, viewDir)) * 2.0f;
    }
    return float4(finalColor, finalAlpha);
}
