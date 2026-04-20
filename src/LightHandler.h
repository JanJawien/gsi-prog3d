#pragma once

#include <string>
#include "StructDef.h"

class LightHandler
{
private:
    std::vector<Light> lightsScene;
    std::vector<Light> lightsOther;
    int m_lightCount = 0;
public:
    // Init functions
    void InitLights();

    // Getters
    int GetLightCount();
    void UpdateLights(Light outLights[MAX_LIGHTS], int& outCount);

    // Lighting effects
    void ToggleAmbientLight();
    void ToggleSceneLights();
};