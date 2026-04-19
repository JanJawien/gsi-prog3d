#pragma once

#include <vector>
#include <cstdint>
#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>
#include <dxgi1_6.h>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT2 uv;
};

struct alignas(256) ObjectConstants
{
    XMFLOAT4X4 world;
    XMFLOAT4X4 worldViewProj;
    XMFLOAT3 lightPosition;
    float lightIntensity;
    XMFLOAT3 cameraPosition;
    float uvScale; 
    XMFLOAT4 baseColor;
};

struct Mesh
{
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
};

struct ObjectRenderData
{
    Mesh mesh;

    ComPtr<ID3D12Resource> vertexBuffer;
    ComPtr<ID3D12Resource> vertexUpload;

    ComPtr<ID3D12Resource> indexBuffer;
    ComPtr<ID3D12Resource> indexUpload;

    D3D12_VERTEX_BUFFER_VIEW vbv{};
    D3D12_INDEX_BUFFER_VIEW ibv{};
    UINT indexCount = 0;

    ComPtr<ID3D12Resource> texture;
    ComPtr<ID3D12Resource> textureUpload;

    D3D12_GPU_DESCRIPTOR_HANDLE srvGpu{};
};

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