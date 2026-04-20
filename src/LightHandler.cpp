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
                { 1.0f, 1.0f, 1.0f }, 20.0f,
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
        { 1.0f, 1.0f, 1.0f }, 20.0f,
        { 0.0f, -1.0f, 0.0f }, GetSpotPower(),
        1, 1, {}
        });
    lightsScene.push_back({
        { 6.0f, 4.5f, 3.0f }, 4.0f,
        { 1.0f, 1.0f, 1.0f }, 20.0f,
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
}

void LightHandler::RemoveSceneLight() {
    SceneLightDistribute(lightsScene.size() - 1);
}



