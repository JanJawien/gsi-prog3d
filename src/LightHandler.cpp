#include "LightHandler.h"

using namespace DirectX;

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
