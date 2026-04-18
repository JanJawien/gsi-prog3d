#pragma once

#include <string>
#include "StructDef.h"

class ModelLoader
{
public:
    static Mesh LoadOBJ(const std::string& filename,
        float offsetX = 0.0f,
        float offsetY = 0.0f,
        float offsetZ = 0.0f);
};