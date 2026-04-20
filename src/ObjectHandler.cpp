#include "ObjectHandler.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

using namespace DirectX;

// ----- private ------

/// <summary>
/// Load a new model from .obj file
/// Use offset parameters to offset loaded model
/// </summary>
Mesh ObjectHandler::LoadGeometry(const std::string& filename,
    float offsetX,
    float offsetY,
    float offsetZ)
{
    Mesh mesh;

    std::ifstream file(filename);
    if (!file.is_open())
        throw std::runtime_error("Failed to open OBJ file.");

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
            for (int i = 0; i < 3; i++)
            {
                std::string vertexData;
                ss >> vertexData;

                std::replace(vertexData.begin(), vertexData.end(), '/', ' ');

                std::stringstream vss(vertexData);

                int vIdx = 0, tIdx = 0;
                vss >> vIdx >> tIdx;

                if (vIdx <= 0 || vIdx > temp_positions.size())
                    throw std::runtime_error("Invalid vertex index in OBJ.");

                if (tIdx <= 0 || tIdx > temp_uvs.size())
                    throw std::runtime_error("Invalid UV index in OBJ.");

                Vertex v;
                v.position = temp_positions[vIdx - 1];
                v.uv = temp_uvs[tIdx - 1];

                mesh.vertices.push_back(v);
                mesh.indices.push_back(static_cast<uint16_t>(mesh.indices.size()));
            }
        }
    }

    return mesh;
}

void ObjectHandler::LoadObject(const std::string& objPath,
    const wchar_t* texturePath,
    UINT index)
{
    if (objects.size() <= index)
        objects.resize(index + 1);

    ObjectRenderData& obj = objects[index];

    obj.mesh = LoadGeometry(objPath);

    if (m_createMeshBuffers)
        m_createMeshBuffers(obj);

    if (m_loadTexture)
        m_loadTexture(texturePath, index, obj);
}

void ObjectHandler::LoadAllObjects() {
    LoadObject("Assets/room.obj", L"Assets/bricks.dds", 0);
    LoadObject("Assets/scene-base.obj", L"Assets/energy.dds", 1);
    LoadObject("Assets/tables-and-chairs.obj", L"Assets/wood.dds", 2);
    LoadObject("Assets/stairs.obj", L"Assets/wood.dds", 3);
    LoadObject("Assets/dj-setup.obj", L"Assets/black.dds", 4);
    LoadObject("Assets/dj-desk.obj", L"Assets/crate.dds", 5);
    LoadObject("Assets/speakers.obj", L"Assets/black.dds", 6);
}

// ===== public =====
// ===== constructors =====

ObjectHandler::ObjectHandler()
{

}

// ===== getters =====

