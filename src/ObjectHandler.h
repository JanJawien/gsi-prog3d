#pragma once

#include "StructDef.h"
#include <string>
#include <functional>

class ObjectHandler
{
private:
    std::vector<ObjectRenderData> objects;

    std::function<void(ObjectRenderData&)> m_createMeshBuffers;
    std::function<void(const wchar_t*, UINT, ObjectRenderData&)> m_loadTexture;

    Mesh LoadGeometry(const std::string& filename,
        float offsetX = 0.0f,
        float offsetY = 0.0f,
        float offsetZ = 0.0f);
    void LoadObject(const std::string& objPath,
        const wchar_t* texturePath,
        UINT index, bool isTransparent = false);

    void CalculateMeshCenter(ObjectRenderData& obj);
    bool IsCameraLookingAtObjectCenter(XMFLOAT3 camPos, XMVECTOR camFwd, ObjectRenderData obj, float maxDistance, float minDot);

public:
    // Constructor
    ObjectHandler();
    void LoadAllObjects();

    // Function injectors
    void SetMeshBufferFunc(std::function<void(ObjectRenderData&)> func) {
        m_createMeshBuffers = func; }
    void SetTextureFunc(std::function<void(const wchar_t*, UINT, ObjectRenderData&)> func) {
        m_loadTexture = func; }

    // Getters
    std::vector<ObjectRenderData>& GetObjects() { return objects; }

    // Interaction
    int GetClickedObjectIndex(XMFLOAT3 cameraPos, XMVECTOR cameraForward);
};