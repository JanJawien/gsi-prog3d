#pragma once

#include <string>
#include "StructDef.h"

class ModelLoader
{
public:
    static Mesh LoadOBJ(const std::string& filename, float offsetX, float offsetY, float offsetZ);
};