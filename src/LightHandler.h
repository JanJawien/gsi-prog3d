#pragma once

#include <string>
#include "StructDef.h"

const int MIN_LIGHTS_SCENE = 2;
const int MAX_LIGHTS_SCENE = 9;

class LightHandler
{
private:
    std::vector<Light> lightsScene;
    std::vector<Light> lightsOther;
    bool isSceneLightBlurOn = true;
    XMFLOAT3 sceneLightBaseColor = {1.0, 1.0, 1.0};

    void SceneLightDistribute(int targetCount);
    float GetSpotPower();
    void InitLights();

public:
    // Constructor
    LightHandler();

    // Getters
    int GetLightCount();
    void UpdateLights(Light outLights[MAX_LIGHTS], int& outCount);

    // Lighting effects
    void ToggleAmbientLight();
    void ToggleSceneLights();
    void ToggleSceneLightBlur();
    void AddSceneLight();
    void RemoveSceneLight();
    void _TEMP_SetSceneLightBaseColor(float r, float g, float b);
};