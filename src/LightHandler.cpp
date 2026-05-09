#include "LightHandler.h"
#include <algorithm>

using namespace DirectX;

// ----- private -----
// ----- utils -----

void LightHandler::SceneLightDistribute(int targetCount){
    constexpr float x = 6.0f;
    constexpr float y = 4.5f;
    const float zMin = -3.0f;
    const float zMax = 3.0f;

    targetCount = std::clamp(targetCount, MIN_LIGHTS_SCENE, MAX_LIGHTS_SCENE);
    lightsScene.resize(targetCount);

    const int count = static_cast<int>(lightsScene.size());

    for (int i = 0; i < count; ++i)
    {
        float t = (float)i / (count - 1);
        float z = zMin + t * (zMax - zMin);

        if (i + 1 == count) {
            lightsScene[i] = {
                { x, y, z }, 4.0f,
                sceneLightBaseColor, 20.0f,
                { 0.0f, -1.0f, 0.0f }, GetSpotPower(),
                1, 1, {}
            };
        }
        else {
            lightsScene[i].position.z = z;
        }
    }
}

void LightHandler::InitLights() {
    // Ambient light
    lightsOther.push_back({
        { -3.0f, 4.5f, 0.0f }, 5.0f,
        { 1.0f, 0.8f, 0.6f }, 20.0f,
        { 0,0,0 }, 1.0f,
        0, 1, {}
        });

    // Scene lights
    lightsScene.push_back({
        { 6.0f, 4.5f, -3.0f }, 4.0f,
        sceneLightBaseColor, 20.0f,
        { 0.0f, -1.0f, 0.0f }, GetSpotPower(),
        1, 1, {}
        });
    lightsScene.push_back({
        { 6.0f, 4.5f, 3.0f }, 4.0f,
        sceneLightBaseColor, 20.0f,
        { 0.0f, -1.0f, 0.0f }, GetSpotPower(),
        1, 1, {}
        });
}

// ----- getters -----

float LightHandler::GetSpotPower() { return isSceneLightBlurOn ? 20.0f : 500.0f;  }





// ===== public =====
// ===== constructors =====

LightHandler::LightHandler()
{
    InitLights();
}

// ===== getters =====

int LightHandler::GetLightCount() { return min(lightsScene.size()+lightsOther.size(), MAX_LIGHTS); }

XMMATRIX LightHandler::GetSpotlightPos(int idx)
{
    if (idx >= (int)lightsScene.size())
        return XMMatrixTranslation(0.0f, -100.0f, 0.0f);

    return XMMatrixTranslation(
        lightsScene[idx].position.x,
        lightsScene[idx].position.y,
        lightsScene[idx].position.z
    );
}

XMMATRIX LightHandler::GetSpotlightRot(int idx)
{
    if (idx >= (int)lightsScene.size())
        return XMMatrixIdentity();

    // Mesh default forward/down direction
    // (no rotation when spotlight points downward)
    XMVECTOR defaultDir =
        XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);

    // Spotlight direction
    XMVECTOR targetDir =
        XMVector3Normalize(
            XMLoadFloat3(&lightsScene[idx].direction));

    float dot =
        XMVectorGetX(
            XMVector3Dot(defaultDir, targetDir));

    dot = std::clamp(dot, -1.0f, 1.0f);

    // Same direction -> identity
    if (fabs(dot - 1.0f) < 0.0001f)
    {
        return XMMatrixIdentity();
    }

    // Opposite direction -> 180 deg rotation
    if (fabs(dot + 1.0f) < 0.0001f)
    {
        return XMMatrixRotationX(XM_PI);
    }

    XMVECTOR axis =
        XMVector3Normalize(
            XMVector3Cross(defaultDir, targetDir));

    float angle = acosf(dot);

    return XMMatrixRotationAxis(axis, angle);
}

void LightHandler::UpdateLights(Light outLights[MAX_LIGHTS], int& outCount) {
    size_t i = 0;

    for (const auto& l : lightsScene) {
        if (i >= MAX_LIGHTS) break;
        outLights[i++] = l;
    }
    for (const auto& l : lightsOther) {
        if (i >= MAX_LIGHTS) break;
        outLights[i++] = l;
    }
    for (; i < MAX_LIGHTS; ++i) {
        outLights[i] = {}; // zero-init
    }
}

// ===== effects =====

void LightHandler::ToggleAmbientLight() {
    lightsOther[0].isEnabled = !lightsOther[0].isEnabled;
}

void LightHandler::ToggleSceneLights() {
    for (auto& l : lightsScene) { l.isEnabled = !l.isEnabled; }
}

void LightHandler::ToggleSceneLightBlur() {
    isSceneLightBlurOn = !isSceneLightBlurOn;
    for (auto& l : lightsScene) { l.spotPower = GetSpotPower(); }
}

void LightHandler::AddSceneLight() {
    SceneLightDistribute(lightsScene.size() + 1);
    ChangeLightEffect(lightEffectIdx);
}

void LightHandler::RemoveSceneLight() {
    SceneLightDistribute(lightsScene.size() - 1);
    ChangeLightEffect(lightEffectIdx);
}

void LightHandler::ChangeLightEffect(int effectIndex) {
    lightEffectIdx = effectIndex;
    lightEffectStartTime = -1.0;
}

void LightHandler::UpdateSpotlights(float totalTime) {
    if (lightEffectStartTime == -1.0) {
        lightEffectStartTime = totalTime;
    }
    float effectTime = totalTime - lightEffectStartTime;

    switch (lightEffectIdx) {
    case 0:
        if (effectTime == 0.0) {
            for (auto& l : lightsScene) {
                l.color = { 1.0f, 1.0f, 1.0f };
                l.direction = { 0.0f, -1.0f, 0.0f };
                l.isEnabled = true;
            }
        }
        else {}
        break;


    case 1:
        if (effectTime == 0.0) {
            for (auto& l : lightsScene) {
                l.color = { 1.0f, 0.0f, 0.0f };
                l.direction = { 0.0f, -1.0f, 0.0f };
                l.isEnabled = true;
            }
        }
        else {
            const float huePeriod = 10.0f;
            const float tiltPeriod = 5.0f;

            float hueT = fmod(effectTime, huePeriod) / huePeriod;

            float tiltT = cos((effectTime / tiltPeriod) * 2.0f * 3.14159265f) * 0.5f + 0.5f;

            // angle from straight down (-90°) to -30°
            float angle = -60.0f + (0.0f - -60.0f) * tiltT;
            float rad = angle * 3.14159265f / 180.0f;

            // direction (tilted toward +X axis)
            XMFLOAT3 tiltedDirF(
                sinf(rad),
                -cosf(rad),
                0.0f
            );

            // normalize direction
            XMVECTOR dir = XMVector3Normalize(XMLoadFloat3(&tiltedDirF));
            XMFLOAT3 tiltedDir;
            XMStoreFloat3(&tiltedDir, dir);

            // HSV → RGB
            auto HueToRGB = [](float h)
                {
                    float r = fabs(h * 6.0f - 3.0f) - 1.0f;
                    float g = 2.0f - fabs(h * 6.0f - 2.0f);
                    float b = 2.0f - fabs(h * 6.0f - 4.0f);

                    return XMFLOAT3(
                        std::clamp(r, 0.0f, 1.0f),
                        std::clamp(g, 0.0f, 1.0f),
                        std::clamp(b, 0.0f, 1.0f)
                    );
                };

            XMFLOAT3 color = HueToRGB(hueT);

            for (auto& l : lightsScene)
            {
                l.color = color;
                l.direction = tiltedDir;
                l.isEnabled = true;
            }
        }
        break;
    }
}



