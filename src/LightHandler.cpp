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

        lightsScene[i] = {
             { x, y, z }, 4.0f,
             sceneLightBaseColor, 20.0f,
             { 0.0f, -1.0f, 0.0f }, GetSpotPower(),
             1, 1, {}
        };
    }
}

void LightHandler::InitLights() {
    // Ambient light
    lightsOther.push_back({
        { -3.0f, 4.5f, 0.0f }, 3.0f,
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

void AnglesToRotVector(XMFLOAT3* destVec, float angleVertical, float angleHorizontal) {
    return XMStoreFloat3(destVec, XMVector3Normalize(XMVectorSet(
        -sinf(angleVertical * 3.14159265f / 180.0f),
        -cosf(angleVertical * 3.14159265f / 180.0f),
        tanf(angleHorizontal * 3.14159265f / 180.0f), 0.0f)));
}

XMFLOAT3 FloatToHue(float t) {
    float r = fabs(t * 6.0f - 3.0f) - 1.0f;
    float g = 2.0f - fabs(t * 6.0f - 2.0f);
    float b = 2.0f - fabs(t * 6.0f - 4.0f);

    return XMFLOAT3(
        std::clamp(r, 0.0f, 1.0f),
        std::clamp(g, 0.0f, 1.0f),
        std::clamp(b, 0.0f, 1.0f)
    );
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

    XMVECTOR defaultDir =
        XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);
    XMVECTOR targetDir =
        XMVector3Normalize(
            XMLoadFloat3(&lightsScene[idx].direction));

    float dot =
        XMVectorGetX(
            XMVector3Dot(defaultDir, targetDir));
    dot = std::clamp(dot, -1.0f, 1.0f);

    if (fabs(dot - 1.0f) < 0.0001f)
        return XMMatrixIdentity();
    if (fabs(dot + 1.0f) < 0.0001f)
        return XMMatrixRotationX(XM_PI);

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


void LightHandler::ChangeLightEffectNext(){
    ChangeLightEffect(lightEffectIdx + 1);
}
void LightHandler::ChangeLightEffectPrev(){
    ChangeLightEffect(lightEffectIdx - 1);
}

void LightHandler::ChangeLightEffect(int effectIndex) {
    switch (effectIndex) {
    case 0: // White, static
        for (auto& l : lightsScene) {
            l.color = { 1.0f, 1.0f, 1.0f };
            l.direction = { 0.0f, -1.0f, 0.0f };
            l.isEnabled = true;
        }
        lightEffectPeriod = -1;
        angVFunc = NULL;
        angHFunc = NULL;
        colorFunc = NULL;
        break;

    case 1: // Hue pan, wave to front
        for (auto& l : lightsScene) {
            l.color = { 1.0f, 0.0f, 0.0f };
            l.direction = { 0.0f, -1.0f, l.position.z / 3 };
            l.isEnabled = true;
        }
        lightEffectPeriod = 16;
        angVFunc = [](float z, float t, int i) { return 60.0f * (-cos((t + abs(z / 10)) * XM_2PI) / 2 + 0.5f); };
        angHFunc = [](float z, float t, int i) { return 45.0f * z / 3;};
        colorFunc = [](float z, float t, int i) { return FloatToHue(t); };
        break;

    case 2: // White flashing, 45deg to front and slight apart
        for (auto& l : lightsScene) {
            l.color = { 1.0f, 1.0f, 1.0f };
            l.direction = { -1.0f, -1.0f, l.position.z / 24 };
            l.isEnabled = true;
        }
        lightEffectPeriod = 2;
        angVFunc = NULL;
        angHFunc = NULL;
        colorFunc = [](float z, float t, int i) {
            if((t>0.5) ^ (i%2==0)) return XMFLOAT3{ 1.0f, 1.0f, 1.0f };
            else return XMFLOAT3{ 0.0f, 0.0f, 0.0f };
            };
        break;

    case 3: // Different colors spinning in 8 shape
        for (auto& l : lightsScene) {
            l.color = FloatToHue(l.position.z/6 + 0.5f);
            l.direction = { -1.0f, -1.0f, 0.0f };
            l.isEnabled = true;
        }
        lightEffectPeriod = 16;
        angVFunc = [](float z, float t, int i) { return 30.0f + 15.0f * cos(2*t * XM_2PI + z*8); };
        angHFunc = [](float z, float t, int i) { return 15.0f * sin(t * XM_2PI + z*8); };
        colorFunc = NULL;
        break;

    case 4: // Quick move to back
        for (auto& l : lightsScene) {
            l.color = { 1.0f, 1.0f, 1.0f };
            l.direction = { -1.0f, -1.0f, 0.0f };
            l.isEnabled = true;
        }
        lightEffectPeriod = 8;
        angVFunc = [](float z, float t, int i) { return 75.0f * (t>1.0/6 && t<5.0/6 ? 1 : sin(t*3*3.14159)); };
        angHFunc = [](float z, float t, int i) { return 0.0f; };
        colorFunc = NULL;
        break;

    case 5: // BLink with random rot toward center
        for (auto& l : lightsScene) {
            l.color = { 1.0f, 1.0f, 0.0f };
            l.direction = { -1.0f, -1.0f, 0.0f };
            l.isEnabled = true;
        }
        lightEffectPeriod = 8;
        angVFunc = [](float z, float t, int i) { return 15.0f; };
        angHFunc = [](float z, float t, int i) { return (10.0f - 60.0f * sin(1.57f+ 8*floor(t*4))) * z/3; };
        colorFunc = [](float z, float t, int i) {
            if (((int)(t*8))%2 == 1) return FloatToHue(0.25f*floor(t*4));
            else return XMFLOAT3{ 0.0f, 0.0f, 0.0f };
            };
        break;

    default: // Out of range, return
        return;
    }

    lightEffectIdx = effectIndex;
    lightEffectStartTime = -1.0;
}

void LightHandler::UpdateSpotlights(float totalTime) {
    if (lightEffectBPM <= 0 || lightEffectPeriod <= 0)
        return;

    if (lightEffectStartTime == -1.0) 
        lightEffectStartTime = totalTime;

    float effectTime = totalTime - lightEffectStartTime;
    double fTime = fmod(effectTime / (60.0 / lightEffectBPM * lightEffectPeriod), 1);

    int lightIdx = 0;
    for (auto& l : lightsScene)
    {
        if (colorFunc != NULL) 
            l.color = colorFunc(l.position.z, fTime, lightIdx);
        if (angVFunc != NULL && angHFunc != NULL) {
            XMFLOAT3 tiltedDir;
            float angV = angVFunc(l.position.z, fTime, lightIdx);
            float angH = angHFunc(l.position.z, fTime, lightIdx);
            AnglesToRotVector(&tiltedDir, angV, angH);
            l.direction = tiltedDir;
        }
        ++lightIdx;
    }
}



