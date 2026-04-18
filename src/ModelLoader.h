#pragma once

#include <string>
#include "StructDef.h"

class ModelLoader
{
public:
    static Mesh CreateCubeMesh();
    static Mesh LoadOBJ(const std::string& filename);
};