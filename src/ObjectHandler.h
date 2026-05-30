#pragma once

#include "StructDef.h"
#include <string>
#include <functional>
#include <vector>
#include <DirectXMath.h>

class ObjectHandler
{
private:
    // list of all scene objects/models
    std::vector<ObjectRenderData> objects;

    // functions set from main.cpp so ObjectHandler can create GPU buffers and textures
    std::function<void(ObjectRenderData&)> m_createMeshBuffers;
    std::function<void(const wchar_t*, UINT, ObjectRenderData&)> m_loadTexture;

    // loads geometry from an .obj file
    // offsetX/Y/Z can move the model during loading
    Mesh LoadGeometry(
        const std::string& filename,
        float offsetX = 0.0f,
        float offsetY = 0.0f,
        float offsetZ = 0.0f
    );

    // adds one object to the scene
    // index = object number, isTransparent = transparency, isLightCone = visible light cone
    void LoadObject(
        const std::string& objPath,
        const wchar_t* texturePath,
        UINT index,
        bool isTransparent = false,
        bool isLightCone = false
    );

    // calculates model center, used for example in rotations
    void CalculateMeshCenter(ObjectRenderData& obj);

    // checks if the camera ray hits a triangle of a model
    bool RayIntersectsTriangle(
        DirectX::XMVECTOR rayOrigin,
        DirectX::XMVECTOR rayDir,
        DirectX::XMVECTOR v0,
        DirectX::XMVECTOR v1,
        DirectX::XMVECTOR v2,
        float& outDistance
    );

public:
    ObjectHandler();

    // loads all models from Assets
    void LoadAllObjects();

    // passes GPU buffer creation function from main.cpp
    void SetMeshBufferFunc(std::function<void(ObjectRenderData&)> func)
    {
        m_createMeshBuffers = func;
    }

    // passes texture loading function from main.cpp
    void SetTextureFunc(std::function<void(const wchar_t*, UINT, ObjectRenderData&)> func)
    {
        m_loadTexture = func;
    }

    // access to the object list, for changing position/visibility etc.
    std::vector<ObjectRenderData>& GetObjects()
    {
        return objects;
    }

    // returns index of the object the camera is looking at, or -1 if nothing is hit
    int GetClickedObjectIndex(
        DirectX::XMFLOAT3 cameraPos,
        DirectX::XMVECTOR cameraForward
    );
};
