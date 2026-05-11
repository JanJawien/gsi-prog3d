#pragma once

#include "StructDef.h"
#include <string>
#include <functional>
#include <vector>
#include <DirectXMath.h>

class ObjectHandler
{
private:
    std::vector<ObjectRenderData> objects;

    std::function<void(ObjectRenderData&)> m_createMeshBuffers;
    std::function<void(const wchar_t*, UINT, ObjectRenderData&)> m_loadTexture;

    Mesh LoadGeometry(
        const std::string& filename,
        float offsetX = 0.0f,
        float offsetY = 0.0f,
        float offsetZ = 0.0f
    );

    void LoadObject(
        const std::string& objPath,
        const wchar_t* texturePath,
        UINT index,
        bool isTransparent = false,
        bool isLightCone = false
    );

    void CalculateMeshCenter(ObjectRenderData& obj);

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

    void LoadAllObjects();

    void SetMeshBufferFunc(std::function<void(ObjectRenderData&)> func)
    {
        m_createMeshBuffers = func;
    }

    void SetTextureFunc(std::function<void(const wchar_t*, UINT, ObjectRenderData&)> func)
    {
        m_loadTexture = func;
    }

    std::vector<ObjectRenderData>& GetObjects()
    {
        return objects;
    }

    int GetClickedObjectIndex(
        DirectX::XMFLOAT3 cameraPos,
        DirectX::XMVECTOR cameraForward
    );
};