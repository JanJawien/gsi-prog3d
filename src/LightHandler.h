#pragma once

#include <string>
#include "StructDef.h"
#include <functional>

const int MIN_LIGHTS_SCENE = 2;
const int MAX_LIGHTS_SCENE = 9;

class LightHandler
{
private:
    std::vector<Light> lightsScene;
    std::vector<Light> lightsOther;
    bool isSceneLightBlurOn = true;
    XMFLOAT3 sceneLightBaseColor = {1.0, 1.0, 1.0};

    int lightEffectIdx = 0;
    int lightEffectBPM = 120;
    int lightEffectPeriod = 0;
    float lightEffectStartTime = -1.0;
    std::function<float(float, float, int)> angVFunc;
    std::function<float(float, float, int)> angHFunc;
    std::function<XMFLOAT3(float, float, int)> colorFunc;

    void SceneLightDistribute(int targetCount);
    float GetSpotPower();
    void InitLights();

public:
    // Constructor
    LightHandler();

    // Getters
    int GetLightCount();
    XMMATRIX GetSpotlightPos(int idx);
    XMMATRIX GetSpotlightRot(int idx);
    void UpdateLights(Light outLights[MAX_LIGHTS], int& outCount);

    // Lighting effects
    void ToggleAmbientLight();
    void ToggleSceneLights();
    void ToggleSceneLightBlur();
    void AddSceneLight();
    void RemoveSceneLight();
    void ChangeLightEffect(int effectIndex);
    void ChangeLightEffectNext();
    void ChangeLightEffectPrev();
    void UpdateSpotlights(float totalTime);
};