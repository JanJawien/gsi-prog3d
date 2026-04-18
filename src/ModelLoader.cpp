#include "ModelLoader.h"
#include "StructDef.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

using namespace DirectX;

/// <summary>
/// Load a new model from .obj file
/// Use offset parameters to offset loaded model
/// </summary>
Mesh ModelLoader::LoadOBJ(const std::string& filename,
    float offsetX = 0.0f,
    float offsetY = 0.0f,
    float offsetZ = 0.0f)
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