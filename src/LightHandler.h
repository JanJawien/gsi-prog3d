#pragma once

#include <string>
#include "StructDef.h"
#include <functional>

// minimum and maximum number of scene spotlights
const int MIN_LIGHTS_SCENE = 2;
const int MAX_LIGHTS_SCENE = 9;

class LightHandler
{
private:
    // moving scene spotlights
    std::vector<Light> lightsScene;

    // other lights, for example ambient/general light
    std::vector<Light> lightsOther;

    // true = softer/blurry spotlight cone
    bool isSceneLightBlurOn = true;

    // base color of scene spotlights
    XMFLOAT3 sceneLightBaseColor = { 1.0, 1.0, 1.0 };

    // current light effect index
    int lightEffectIdx = 0;

    // light effect tempo, can be changed to match music
    int lightEffectBPM = 135;

    // effect cycle length
    int lightEffectPeriod = 0;

    // start time of current effect
    float lightEffectStartTime = -1.0;

    // effect functions: vertical angle, horizontal angle and color
    std::function<float(float, float, int)> angVFunc;
    std::function<float(float, float, int)> angHFunc;
    std::function<XMFLOAT3(float, float, int)> colorFunc;

    // distributes scene lights within MIN/MAX limits
    void SceneLightDistribute(int targetCount);

    // returns spotlight cone sharpness
    float GetSpotPower();

    // creates initial lights
    void InitLights();

public:
    // Constructor
    LightHandler();

    // Setters
    // sets light effect tempo
    void SetTempo(int tempo) { lightEffectBPM = tempo; }

    // Getters
    // number of lights sent to shader
    int GetLightCount();

    // position matrix for spotlight lamp/cone model
    XMMATRIX GetSpotlightPos(int idx);

    // rotation matrix for spotlight lamp/cone model
    XMMATRIX GetSpotlightRot(int idx);

    // copies lights to renderer array
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

    // updates spotlight animation every frame
    void UpdateSpotlights(float totalTime);
};
