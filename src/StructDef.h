#pragma once

#include <vector>
#include <cstdint>
#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>
#include <dxgi1_6.h>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

// one model vertex: position + texture UV
struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT2 uv;
};

// maximum number of lights sent to the shader
// must match Light lights[16] in Shader.hlsl
const int MAX_LIGHTS = 16;

struct Light
{
    // light position
    XMFLOAT3 position;
    // light brightness
    float intensity;

    // RGB light color
    XMFLOAT3 color;
    // light range
    float range;

    // light direction, important for spotlights
    XMFLOAT3 direction;
    // spotlight cone sharpness
    float spotPower;

    // 0 = general/ambient, 1 = spotlight
    int type;
    // 0 = disabled, 1 = enabled
    int isEnabled;
    XMFLOAT2 pad;
};

// data sent to shader separately for every object
struct alignas(256) ObjectConstants
{
    XMFLOAT4X4 world;
    XMFLOAT4X4 worldViewProj;

    // camera position, used for lighting and transparency
    XMFLOAT3 cameraPosition;
    // texture tiling/repetition
    float uvScale;

    // base object color
    XMFLOAT4 baseColor;

    // lights passed to shader
    Light lights[MAX_LIGHTS];
    int lightCount;
    int isLightCone;
    XMFLOAT2 padding;
};

struct Mesh
{
    // model geometry
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
};

struct ObjectRenderData
{
    // model data
    Mesh mesh;

    // GPU buffers for vertices
    ComPtr<ID3D12Resource> vertexBuffer;
    ComPtr<ID3D12Resource> vertexUpload;

    // GPU buffers for indices
    ComPtr<ID3D12Resource> indexBuffer;
    ComPtr<ID3D12Resource> indexUpload;

    D3D12_VERTEX_BUFFER_VIEW vbv{};
    D3D12_INDEX_BUFFER_VIEW ibv{};
    UINT indexCount = 0;

    // object texture
    ComPtr<ID3D12Resource> texture;
    ComPtr<ID3D12Resource> textureUpload;

    D3D12_GPU_DESCRIPTOR_HANDLE srvGpu{};

    // model center, useful for rotations and picking
    XMFLOAT3 meshCenter = { 0.0f, 0.0f, 0.0f };

    // object world matrix: position/rotation/scale
    // also used by ray picking to test transformed geometry
    XMFLOAT4X4 worldMatrix =
    {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    // transparent object uses different pipeline
    bool isTransparent = false;

    // true if object is a visible light cone
    bool isLightCone = false;

    // false = object is not drawn and cannot be selected
    bool isVisible = true;
};

// helper structures for manual DDS loading
struct DDS_PIXELFORMAT
{
    uint32_t size;
    uint32_t flags;
    uint32_t fourCC;
    uint32_t rgbBitCount;
    uint32_t rBitMask;
    uint32_t gBitMask;
    uint32_t bBitMask;
    uint32_t aBitMask;
};

struct DDS_HEADER
{
    uint32_t size;
    uint32_t flags;
    uint32_t height;
    uint32_t width;
    uint32_t pitchOrLinearSize;
    uint32_t depth;
    uint32_t mipMapCount;
    uint32_t reserved1[11];
    DDS_PIXELFORMAT ddspf;
    uint32_t caps;
    uint32_t caps2;
    uint32_t caps3;
};
