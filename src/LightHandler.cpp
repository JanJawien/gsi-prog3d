#include "LightHandler.h"
#include <algorithm>

using namespace DirectX;

// ----- private -----

void LightHandler::SceneLightDistribute(int targetCount){
    int MIN_LIGHTS = 2;
    constexpr float x = 6.0f;
    constexpr float y = 4.5f;
    const float zMin = -3.0f;
    const float zMax = 3.0f;

    targetCount = std::clamp(targetCount, MIN_LIGHTS, MAX_LIGHTS);
    lightsScene.resize(targetCount);

    const int count = static_cast<int>(lightsScene.size());

    for (int i = 0; i < count; ++i)
    {
        float t = (count == 1) ? 0.5f : (float)i / (count - 1);
        float z = zMin + t * (zMax - zMin);

        lightsScene[i] = {
            { x, y, z }, 4.0f,              // position + intensity
            { 1.0f, 1.0f, 1.0f }, 20.0f,    // color + range
            { 0.0f, -1.0f, 0.0f }, 10.0f,   // direction + spot power
            1, 1, {}                        // type + padding
        };
    }
}

// ----- public -----
// ----- init -----

void LightHandler::InitLights() {
    // Point light
    lightsOther.push_back({
        { -3.0f, 4.5f, 0.0f }, 5.0f,
        { 1.0f, 0.8f, 0.6f }, 20.0f,
        { 0,0,0 }, 1.0f,
        0, 1, {}
        });

    // Spot light
    lightsScene.push_back({
        { 6.0f, 4.5f, -2.0f }, 4.0f,
        { 1.0f, 1.0f, 1.0f }, 20.0f,
        { 0,-1,0 }, 10.0f,
        1, 1, {}
        });

    // Spot light
    lightsScene.push_back({
        { 6.0f, 4.5f, 2.0f }, 4.0f,
        { 1.0f, 1.0f, 1.0f }, 20.0f,
        { 0,-1,0 }, 10.0f,
        1, 1, {}
        });
    m_lightCount = 3;
}

// ----- getters -----

int LightHandler::GetLightCount() { return min(lightsScene.size()+lightsOther.size(), MAX_LIGHTS); }

void LightHandler::UpdateLights(Light outLights[MAX_LIGHTS], int& outCount) {
    size_t i = 0;

    // Fill from scene lights
    for (const auto& l : lightsScene) {
        if (i >= MAX_LIGHTS) break;
        outLights[i++] = l;
    }

    // Fill from other lights
    for (const auto& l : lightsOther) {
        if (i >= MAX_LIGHTS) break;
        outLights[i++] = l;
    }

    outCount = static_cast<int>(i);

    // Optional: zero remaining slots (VERY good for GPU safety)
    for (; i < MAX_LIGHTS; ++i) {
        outLights[i] = {}; // zero-init
    }
}

// ----- effects -----

void LightHandler::ToggleAmbientLight() {
    lightsScene[0].isEnabled = !lightsScene[0].isEnabled;
}

void LightHandler::ToggleSceneLights() {
    for (auto& l : lightsScene) { l.isEnabled = !l.isEnabled; }
}

void LightHandler::AddSceneLight() {
    SceneLightDistribute(lightsScene.size() + 1);
}

void LightHandler::RemoveSceneLight() {
    SceneLightDistribute(lightsScene.size() - 1);
}



