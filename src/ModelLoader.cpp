#include "ModelLoader.h"
#include "StructDef.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

using namespace DirectX;

// ====== TODO - REPLACE WITH BETTER MODEL
/// <summary>
/// Generate cube hall model
/// </summary>
Mesh ModelLoader::CreateCubeMesh()
{
    Mesh m;
    m.vertices = {
        {{-1,-1,-1},{0,1}}, {{-1, 1,-1},{0,0}}, {{ 1, 1,-1},{1,0}}, {{ 1,-1,-1},{1,1}},
        {{-1,-1, 1},{1,1}}, {{ 1,-1, 1},{0,1}}, {{ 1, 1, 1},{0,0}}, {{-1, 1, 1},{1,0}},
        {{-1, 1,-1},{0,1}}, {{-1, 1, 1},{0,0}}, {{ 1, 1, 1},{1,0}}, {{ 1, 1,-1},{1,1}},
        {{-1,-1,-1},{1,1}}, {{ 1,-1,-1},{0,1}}, {{ 1,-1, 1},{0,0}}, {{-1,-1, 1},{1,0}},
        {{-1,-1, 1},{0,1}}, {{-1, 1, 1},{0,0}}, {{-1, 1,-1},{1,0}}, {{-1,-1,-1},{1,1}},
        {{ 1,-1,-1},{0,1}}, {{ 1, 1,-1},{0,0}}, {{ 1, 1, 1},{1,0}}, {{ 1,-1, 1},{1,1}}
    };
    m.indices = {
        0,1,2, 0,2,3,
        4,5,6, 4,6,7,
        8,9,10, 8,10,11,
        12,13,14, 12,14,15,
        16,17,18, 16,18,19,
        20,21,22, 20,22,23
    };
    return m;
}

/// <summary>
/// Load a new model from .obj file
/// </summary>
Mesh ModelLoader::LoadOBJ(const std::string& filename)
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