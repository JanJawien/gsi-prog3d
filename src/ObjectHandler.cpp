#include "ObjectHandler.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <limits>

using namespace DirectX;


// ----- private ------

Mesh ObjectHandler::LoadGeometry(
    const std::string& filename,
    float offsetX,
    float offsetY,
    float offsetZ)
{
    Mesh mesh;

    std::ifstream file(filename);
    if (!file.is_open())
        throw std::runtime_error("Failed to open OBJ file: " + filename);

    std::vector<XMFLOAT3> temp_positions;
    std::vector<XMFLOAT2> temp_uvs;

    std::string line;
    while (std::getline(file, line))
    {
        std::stringstream ss(line);
        std::string prefix;
        ss >> prefix;

        if (prefix == "v")
        {
            XMFLOAT3 pos;
            ss >> pos.x >> pos.y >> pos.z;

            pos.x += offsetX;
            pos.y += offsetY;
            pos.z += offsetZ;

            temp_positions.push_back(pos);
        }
        else if (prefix == "vt")
        {
            XMFLOAT2 uv;
            ss >> uv.x >> uv.y;

            uv.y = 1.0f - uv.y;

            temp_uvs.push_back(uv);
        }
        else if (prefix == "f")
        {
            std::vector<Vertex> faceVertices;
            std::string vertexStr;

            while (ss >> vertexStr)
            {
                int vIdx = 0;
                int tIdx = 0;

                size_t slash1 = vertexStr.find('/');
                size_t slash2 = vertexStr.find('/', slash1 + 1);

                if (slash1 == std::string::npos)
                {
                    vIdx = std::stoi(vertexStr);
                }
                else
                {
                    vIdx = std::stoi(vertexStr.substr(0, slash1));

                    if (slash2 == std::string::npos)
                    {
                        std::string uvPart = vertexStr.substr(slash1 + 1);
                        if (!uvPart.empty())
                            tIdx = std::stoi(uvPart);
                    }
                    else
                    {
                        if (slash2 > slash1 + 1)
                        {
                            std::string uvPart = vertexStr.substr(
                                slash1 + 1,
                                slash2 - slash1 - 1
                            );

                            if (!uvPart.empty())
                                tIdx = std::stoi(uvPart);
                        }
                    }
                }

                Vertex v{};

                if (vIdx > 0 && vIdx <= static_cast<int>(temp_positions.size()))
                    v.position = temp_positions[vIdx - 1];
                else
                    v.position = { 0.0f, 0.0f, 0.0f };

                if (tIdx > 0 && tIdx <= static_cast<int>(temp_uvs.size()))
                    v.uv = temp_uvs[tIdx - 1];
                else
                    v.uv = { 0.0f, 0.0f };

                faceVertices.push_back(v);
            }

            for (size_t i = 1; i + 1 < faceVertices.size(); ++i)
            {
                mesh.vertices.push_back(faceVertices[0]);
                mesh.indices.push_back(static_cast<uint16_t>(mesh.indices.size()));

                mesh.vertices.push_back(faceVertices[i]);
                mesh.indices.push_back(static_cast<uint16_t>(mesh.indices.size()));

                mesh.vertices.push_back(faceVertices[i + 1]);
                mesh.indices.push_back(static_cast<uint16_t>(mesh.indices.size()));
            }
        }
    }

    return mesh;
}

void ObjectHandler::LoadObject(
    const std::string& objPath,
    const wchar_t* texturePath,
    UINT index,
    bool isTransparent,
    bool isLightCone)
{
    if (objects.size() <= index)
        objects.resize(index + 1);

    ObjectRenderData& obj = objects[index];

    obj.mesh = LoadGeometry(objPath);
    CalculateMeshCenter(obj);

    obj.isTransparent = isTransparent;
    obj.isLightCone = isLightCone;

    XMStoreFloat4x4(&obj.worldMatrix, XMMatrixIdentity());

    if (m_createMeshBuffers)
        m_createMeshBuffers(obj);

    if (m_loadTexture)
        m_loadTexture(texturePath, index, obj);
}

void ObjectHandler::CalculateMeshCenter(ObjectRenderData& obj)
{
    if (obj.mesh.vertices.empty())
    {
        obj.meshCenter = XMFLOAT3(0.0f, 0.0f, 0.0f);
        return;
    }

    float minX = obj.mesh.vertices[0].position.x;
    float minY = obj.mesh.vertices[0].position.y;
    float minZ = obj.mesh.vertices[0].position.z;

    float maxX = obj.mesh.vertices[0].position.x;
    float maxY = obj.mesh.vertices[0].position.y;
    float maxZ = obj.mesh.vertices[0].position.z;

    for (const auto& v : obj.mesh.vertices)
    {
        minX = (std::min)(minX, v.position.x);
        minY = (std::min)(minY, v.position.y);
        minZ = (std::min)(minZ, v.position.z);

        maxX = (std::max)(maxX, v.position.x);
        maxY = (std::max)(maxY, v.position.y);
        maxZ = (std::max)(maxZ, v.position.z);
    }

    obj.meshCenter = XMFLOAT3(
        (minX + maxX) * 0.5f,
        (minY + maxY) * 0.5f,
        (minZ + maxZ) * 0.5f
    );
}

bool ObjectHandler::RayIntersectsTriangle(
    XMVECTOR rayOrigin,
    XMVECTOR rayDir,
    XMVECTOR v0,
    XMVECTOR v1,
    XMVECTOR v2,
    float& outDistance)
{
    const float EPSILON = 0.000001f;

    XMVECTOR edge1 = XMVectorSubtract(v1, v0);
    XMVECTOR edge2 = XMVectorSubtract(v2, v0);

    XMVECTOR h = XMVector3Cross(rayDir, edge2);
    float a = XMVectorGetX(XMVector3Dot(edge1, h));

    if (a > -EPSILON && a < EPSILON)
        return false;

    float f = 1.0f / a;

    XMVECTOR s = XMVectorSubtract(rayOrigin, v0);
    float u = f * XMVectorGetX(XMVector3Dot(s, h));

    if (u < 0.0f || u > 1.0f)
        return false;

    XMVECTOR q = XMVector3Cross(s, edge1);
    float v = f * XMVectorGetX(XMVector3Dot(rayDir, q));

    if (v < 0.0f || u + v > 1.0f)
        return false;

    float t = f * XMVectorGetX(XMVector3Dot(edge2, q));

    if (t > EPSILON)
    {
        outDistance = t;
        return true;
    }

    return false;
}


// ===== public =====

ObjectHandler::ObjectHandler()
{
}

void ObjectHandler::LoadAllObjects()
{
    LoadObject("Assets/room.obj", L"Assets/bricks.dds", 0);
    LoadObject("Assets/scene-base.obj", L"Assets/wood.dds", 1);
    LoadObject("Assets/tables-and-chairs.obj", L"Assets/wood.dds", 2);
    LoadObject("Assets/stairs.obj", L"Assets/wood.dds", 3);
    LoadObject("Assets/dj-setup.obj", L"Assets/black.dds", 4);
    LoadObject("Assets/dj-desk.obj", L"Assets/wood.dds", 5);

    LoadObject("Assets/speaker-right.obj", L"Assets/black.dds", 6);
    LoadObject("Assets/speaker-right-back.obj", L"Assets/black.dds", 7);
    LoadObject("Assets/speaker-left.obj", L"Assets/black.dds", 8);
    LoadObject("Assets/speaker-left-back.obj", L"Assets/black.dds", 9);

    LoadObject("Assets/lights-railing-back.obj", L"Assets/railing.dds", 10, true);
    LoadObject("Assets/lights-railing-bottom.obj", L"Assets/railing.dds", 11, true);
    LoadObject("Assets/lights-railing-front.obj", L"Assets/railing.dds", 12, true);

    LoadObject("Assets/light-cone-2.obj", L"Assets/transp.dds", 13, true, true);
    LoadObject("Assets/light-cone-2.obj", L"Assets/transp.dds", 14, true, true);
    LoadObject("Assets/light-cone-2.obj", L"Assets/transp.dds", 15, true, true);
    LoadObject("Assets/light-cone-2.obj", L"Assets/transp.dds", 16, true, true);
    LoadObject("Assets/light-cone-2.obj", L"Assets/transp.dds", 17, true, true);
    LoadObject("Assets/light-cone-2.obj", L"Assets/transp.dds", 18, true, true);
    LoadObject("Assets/light-cone-2.obj", L"Assets/transp.dds", 19, true, true);
    LoadObject("Assets/light-cone-2.obj", L"Assets/transp.dds", 20, true, true);
    LoadObject("Assets/light-cone-2.obj", L"Assets/transp.dds", 21, true, true);

    LoadObject("Assets/spotlight-lamp-2.obj", L"Assets/lamp.dds", 22);
    LoadObject("Assets/spotlight-lamp-2.obj", L"Assets/lamp.dds", 23);
    LoadObject("Assets/spotlight-lamp-2.obj", L"Assets/lamp.dds", 24);
    LoadObject("Assets/spotlight-lamp-2.obj", L"Assets/lamp.dds", 25);
    LoadObject("Assets/spotlight-lamp-2.obj", L"Assets/lamp.dds", 26);
    LoadObject("Assets/spotlight-lamp-2.obj", L"Assets/lamp.dds", 27);
    LoadObject("Assets/spotlight-lamp-2.obj", L"Assets/lamp.dds", 28);
    LoadObject("Assets/spotlight-lamp-2.obj", L"Assets/lamp.dds", 29);
    LoadObject("Assets/spotlight-lamp-2.obj", L"Assets/lamp.dds", 30);

    LoadObject("Assets/bar.obj", L"Assets/wood.dds", 31);
    LoadObject("Assets/door.obj", L"Assets/wood.dds", 32);
    LoadObject("Assets/sofa.obj", L"Assets/fabric.dds", 33);
    LoadObject("Assets/beer1.obj", L"Assets/fabric.dds", 34);
    LoadObject("Assets/beer2.obj", L"Assets/fabric.dds", 35);
}

int ObjectHandler::GetClickedObjectIndex(
    XMFLOAT3 cameraPos,
    XMVECTOR cameraForward)
{
    XMVECTOR rayOrigin = XMLoadFloat3(&cameraPos);
    XMVECTOR rayDir = XMVector3Normalize(cameraForward);

    int bestObjectIndex = -1;
    float closestDistance = (std::numeric_limits<float>::max)();

    for (int objIndex = 0; objIndex < static_cast<int>(objects.size()); ++objIndex)
    {
        ObjectRenderData& obj = objects[objIndex];

        if (!obj.isVisible)
            continue;

        if (obj.mesh.vertices.empty() || obj.mesh.indices.empty())
            continue;

        if (obj.isLightCone)
            continue;

        XMMATRIX world = XMLoadFloat4x4(&obj.worldMatrix);

        for (size_t i = 0; i + 2 < obj.mesh.indices.size(); i += 3)
        {
            uint16_t index0 = obj.mesh.indices[i];
            uint16_t index1 = obj.mesh.indices[i + 1];
            uint16_t index2 = obj.mesh.indices[i + 2];

            if (index0 >= obj.mesh.vertices.size() ||
                index1 >= obj.mesh.vertices.size() ||
                index2 >= obj.mesh.vertices.size())
            {
                continue;
            }

            XMVECTOR v0 = XMLoadFloat3(&obj.mesh.vertices[index0].position);
            XMVECTOR v1 = XMLoadFloat3(&obj.mesh.vertices[index1].position);
            XMVECTOR v2 = XMLoadFloat3(&obj.mesh.vertices[index2].position);

            v0 = XMVector3TransformCoord(v0, world);
            v1 = XMVector3TransformCoord(v1, world);
            v2 = XMVector3TransformCoord(v2, world);

            float hitDistance = 0.0f;

            if (RayIntersectsTriangle(rayOrigin, rayDir, v0, v1, v2, hitDistance))
            {
                if (hitDistance < closestDistance)
                {
                    closestDistance = hitDistance;
                    bestObjectIndex = objIndex;
                }
            }
        }
    }

    return bestObjectIndex;
}