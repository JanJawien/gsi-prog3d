#include "StructDef.h"
#include "ModelLoader.h"
#include "LightHandler.h" 

#include <windows.h>
#include <wrl.h>
#include <shellapi.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <string>
#include <vector>
#include <array>
#include <stdexcept>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <algorithm>

#include <random>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

// Random engine (initialize once, e.g., in constructor or globally)
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_real_distribution<float> speedDist(1.5f, 5.0f);


class Dx12App
{
public:
    bool Initialize(HINSTANCE hInstance, int nCmdShow)
    {
        RegisterWindowClass(hInstance);
        CreateAppWindow(hInstance, nCmdShow);
        LoadPipeline();
        LoadAssets();
        LoadModels();
        return true;
    }

    int Run()
    {
        MSG msg = {};
        LARGE_INTEGER frequency{}, prev{}, now{};
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&prev);

        while (msg.message != WM_QUIT)
        {
            if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            else
            {
                QueryPerformanceCounter(&now);
                float deltaTime = static_cast<float>(now.QuadPart - prev.QuadPart) / static_cast<float>(frequency.QuadPart);
                prev = now;

                Update(deltaTime);
                Render();
            }
        }

        WaitForGpu();
        CloseHandle(m_fenceEvent);
        return static_cast<int>(msg.wParam);
    }

private:
    HWND m_hwnd = nullptr;

    // App params
    static const UINT FrameCount = 2;
    static const UINT Width = 1280;
    static const UINT Height = 720;
    static const UINT ObjectCount = 7; 

    // Object data array
    ObjectRenderData m_objects[ObjectCount];

    // Device Context 
    ComPtr<IDXGIFactory4> m_factory;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    ComPtr<ID3D12DescriptorHeap> m_cbvHeap;
    ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    ComPtr<ID3D12CommandAllocator> m_commandAllocators[FrameCount];
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    ComPtr<ID3D12Resource> m_depthBuffer;
    ComPtr<ID3D12Fence> m_fence;
    ComPtr<ID3D12Resource> m_depthStencil;
    UINT64 m_fenceValues[FrameCount] = {};
    HANDLE m_fenceEvent = nullptr;
    UINT m_frameIndex = 0;
    UINT m_rtvDescriptorSize = 0;
    UINT m_srvDescriptorSize = 0;

    // Camera params (TEMP)
    DirectX::XMFLOAT3 m_cameraPos = { 0.0f, 1.8f, 0.0f }; // Startowa pozycja
    float m_moveSpeed = 2.0f;
    float m_cameraYaw = 3.14f/2;     
    float m_rotationSpeed = 2.0f;

    // Light handler
    LightHandler m_lighting;

    // GPU data
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12Resource> m_constantBuffer;
    uint8_t* m_cbvDataBegin = nullptr;

    // Render region
    D3D12_VIEWPORT m_viewport{};
    D3D12_RECT m_scissorRect{};

    // ----------------------------------------------------------------------------------
    // App initialization functions

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        auto* app = reinterpret_cast<Dx12App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

        switch (message)
        {
        case WM_KEYDOWN:
            if (app) app->HandleKeyboardInput(wParam);
            break;

        case WM_NCCREATE:
            CREATESTRUCT* createStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));
            break;
        }

        if (app) return app->HandleMessage(hWnd, message, wParam, lParam);
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    LRESULT HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }

    void RegisterWindowClass(HINSTANCE hInstance)
    {
        WNDCLASSEX wc = {};
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = L"DX12ThreeSolidsWindowClass";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassEx(&wc);
    }

    void CreateAppWindow(HINSTANCE hInstance, int nCmdShow)
    {
        RECT rc = { 0, 0, static_cast<LONG>(Width), static_cast<LONG>(Height) };
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

        m_hwnd = CreateWindowEx(
            0,
            L"DX12ThreeSolidsWindowClass",
            L"DirectX 12 - 3 bryly z teksturami DDS",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            rc.right - rc.left,
            rc.bottom - rc.top,
            nullptr,
            nullptr,
            hInstance,
            this);

        ShowWindow(m_hwnd, nCmdShow);
    }

    void LoadPipeline()
    {
#if defined(_DEBUG)
        {
            ComPtr<ID3D12Debug> debugController;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
                debugController->EnableDebugLayer();
        }
#endif

        UINT dxgiFactoryFlags = 0;
        ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_factory)));

        ComPtr<IDXGIAdapter1> hardwareAdapter;
        GetHardwareAdapter(hardwareAdapter.ReleaseAndGetAddressOf());

        ThrowIfFailed(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));

        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.BufferCount = FrameCount;
        swapChainDesc.Width = Width;
        swapChainDesc.Height = Height;
        swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.SampleDesc.Count = 1;

        ComPtr<IDXGISwapChain1> swapChain;
        ThrowIfFailed(m_factory->CreateSwapChainForHwnd(
            m_commandQueue.Get(),
            m_hwnd,
            &swapChainDesc,
            nullptr,
            nullptr,
            &swapChain));

        ThrowIfFailed(m_factory->MakeWindowAssociation(m_hwnd, DXGI_MWA_NO_ALT_ENTER));
        ThrowIfFailed(swapChain.As(&m_swapChain));
        m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = FrameCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));
        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

        D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
        srvHeapDesc.NumDescriptors = ObjectCount;
        srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)));
        m_srvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
        cbvHeapDesc.NumDescriptors = ObjectCount;
        cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_cbvHeap)));

        for (UINT i = 0; i < FrameCount; i++)
        {
            ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));

            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
            rtvHandle.ptr += static_cast<SIZE_T>(i) * m_rtvDescriptorSize;
            m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);

            ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[i])));
        }

        D3D12_RESOURCE_DESC depthDesc = {};
        depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthDesc.Width = Width;
        depthDesc.Height = Height;
        depthDesc.DepthOrArraySize = 1;
        depthDesc.MipLevels = 1;
        depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
        depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
        depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
        depthOptimizedClearValue.DepthStencil.Stencil = 0;

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        ThrowIfFailed(m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &depthDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &depthOptimizedClearValue,
            IID_PPV_ARGS(&m_depthBuffer)
        ));

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

        m_device->CreateDepthStencilView(
            m_depthBuffer.Get(),
            &dsvDesc,
            m_dsvHeap->GetCPUDescriptorHandleForHeapStart()
        );

        m_viewport = { 0.0f, 0.0f, static_cast<float>(Width), static_cast<float>(Height), 0.0f, 1.0f };
        m_scissorRect = { 0, 0, static_cast<LONG>(Width), static_cast<LONG>(Height) };

        ThrowIfFailed(m_device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            m_commandAllocators[m_frameIndex].Get(),
            nullptr,
            IID_PPV_ARGS(&m_commandList)));
        ThrowIfFailed(m_commandList->Close());

        ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fenceValues[m_frameIndex] = 1;
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

        D3D12_CLEAR_VALUE depthClearValue = {};
        depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;
        depthClearValue.DepthStencil.Depth = 1.0f;

        D3D12_HEAP_PROPERTIES depthHeapProps = {};
        depthHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        ThrowIfFailed(m_device->CreateCommittedResource(
            &depthHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &depthDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &depthClearValue,
            IID_PPV_ARGS(&m_depthStencil)));

        m_device->CreateDepthStencilView(m_depthStencil.Get(), nullptr, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    }
    

    void LoadAssets()
    {
        D3D12_DESCRIPTOR_RANGE range = {};
        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        range.NumDescriptors = 1;
        range.BaseShaderRegister = 0;
        range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER rootParameters[2] = {};
        rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[0].Descriptor.ShaderRegister = 0;
        rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
        rootParameters[1].DescriptorTable.pDescriptorRanges = &range;
        rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
        rootSignatureDesc.NumParameters = _countof(rootParameters);
        rootSignatureDesc.pParameters = rootParameters;
        rootSignatureDesc.NumStaticSamplers = 1;
        rootSignatureDesc.pStaticSamplers = &sampler;
        rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;



        ComPtr<ID3DBlob> signature, error;
        ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
        ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));

        UINT shaderFlags = 0;
#if defined(_DEBUG)
        shaderFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        ComPtr<ID3DBlob> vertexShader, pixelShader;
        ThrowIfFailed(D3DCompileFromFile(L"Shader.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", shaderFlags, 0, &vertexShader, &error));
        ThrowIfFailed(D3DCompileFromFile(L"Shader.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", shaderFlags, 0, &pixelShader, &error));

        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        psoDesc.pRootSignature = m_rootSignature.Get();
        psoDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
        psoDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; 
        psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
        psoDesc.RasterizerState.DepthClipEnable = TRUE;
        psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.DepthStencilState.DepthEnable = TRUE;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        psoDesc.SampleDesc.Count = 1;
        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));


        const UINT cbSize = (sizeof(ObjectConstants) + 255) & ~255u;
        const UINT totalCbSize = cbSize * ObjectCount;

        D3D12_HEAP_PROPERTIES cbHeapProps = {};
        cbHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC cbDesc = {};
        cbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        cbDesc.Width = totalCbSize;
        cbDesc.Height = 1;
        cbDesc.DepthOrArraySize = 1;
        cbDesc.MipLevels = 1;
        cbDesc.SampleDesc.Count = 1;
        cbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ThrowIfFailed(m_device->CreateCommittedResource(
            &cbHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &cbDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_constantBuffer)));

        ThrowIfFailed(m_constantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_cbvDataBegin)));
        WaitForGpu();
    }

    // ----------------------------------------------------------------------------------
    // Model loading functions

    void LoadModels() {
        //m_objects[0].mesh = ModelLoader::CreateCubeMesh();
        m_objects[0].mesh = ModelLoader::LoadOBJ("Assets/room.obj");
        CreateMeshBuffers(m_objects[0]);
        LoadDDSTexture(L"Assets/bricks.dds", 0, m_objects[0]);

        m_objects[1].mesh = ModelLoader::LoadOBJ("Assets/scene-base.obj");
        CreateMeshBuffers(m_objects[1]);
        LoadDDSTexture(L"Assets/energy.dds", 1, m_objects[1]);

        m_objects[2].mesh = ModelLoader::LoadOBJ("Assets/tables-and-chairs.obj");
        CreateMeshBuffers(m_objects[2]);
        LoadDDSTexture(L"Assets/wood.dds", 2, m_objects[2]);

        m_objects[3].mesh = ModelLoader::LoadOBJ("Assets/stairs.obj");
        CreateMeshBuffers(m_objects[3]);
        LoadDDSTexture(L"Assets/wood.dds", 3, m_objects[3]);

        m_objects[4].mesh = ModelLoader::LoadOBJ("Assets/dj-setup.obj");
        CreateMeshBuffers(m_objects[4]);
        LoadDDSTexture(L"Assets/black.dds", 4, m_objects[4]);

        m_objects[5].mesh = ModelLoader::LoadOBJ("Assets/dj-desk.obj");
        CreateMeshBuffers(m_objects[5]);
        LoadDDSTexture(L"Assets/crate.dds", 5, m_objects[5]);

        m_objects[6].mesh = ModelLoader::LoadOBJ("Assets/speakers.obj");
        CreateMeshBuffers(m_objects[6]);
        LoadDDSTexture(L"Assets/black.dds", 6, m_objects[6]);
    }
    
    void CreateMeshBuffers(ObjectRenderData& obj)
    {
        const UINT vbSize = static_cast<UINT>(obj.mesh.vertices.size() * sizeof(Vertex));
        const UINT ibSize = static_cast<UINT>(obj.mesh.indices.size() * sizeof(uint16_t));

        CreateDefaultBuffer(obj.mesh.vertices.data(), vbSize, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, obj.vertexBuffer, obj.vertexUpload);
        CreateDefaultBuffer(obj.mesh.indices.data(), ibSize, D3D12_RESOURCE_STATE_INDEX_BUFFER, obj.indexBuffer, obj.indexUpload);

        obj.vbv.BufferLocation = obj.vertexBuffer->GetGPUVirtualAddress();
        obj.vbv.SizeInBytes = vbSize;
        obj.vbv.StrideInBytes = sizeof(Vertex);

        obj.ibv.BufferLocation = obj.indexBuffer->GetGPUVirtualAddress();
        obj.ibv.SizeInBytes = ibSize;
        obj.ibv.Format = DXGI_FORMAT_R16_UINT;
        obj.indexCount = static_cast<UINT>(obj.mesh.indices.size());
    }

    template<typename T>
    void CreateDefaultBuffer(const T* data, UINT byteSize, D3D12_RESOURCE_STATES finalState, ComPtr<ID3D12Resource>& defaultBuffer, ComPtr<ID3D12Resource>& uploadBuffer)
    {
        D3D12_HEAP_PROPERTIES defaultHeapProps = {};
        defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC bufferDesc = {};
        bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Width = byteSize;
        bufferDesc.Height = 1;
        bufferDesc.DepthOrArraySize = 1;
        bufferDesc.MipLevels = 1;
        bufferDesc.SampleDesc.Count = 1;
        bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ThrowIfFailed(m_device->CreateCommittedResource(&defaultHeapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&defaultBuffer)));

        D3D12_HEAP_PROPERTIES uploadHeapProps = {};
        uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        ThrowIfFailed(m_device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuffer)));

        void* mappedData = nullptr;
        ThrowIfFailed(uploadBuffer->Map(0, nullptr, &mappedData));
        memcpy(mappedData, data, byteSize);
        uploadBuffer->Unmap(0, nullptr);

        ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());
        ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr));
        m_commandList->CopyBufferRegion(defaultBuffer.Get(), 0, uploadBuffer.Get(), 0, byteSize);

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = defaultBuffer.Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = finalState;
        m_commandList->ResourceBarrier(1, &barrier);

        ThrowIfFailed(m_commandList->Close());
        ID3D12CommandList* lists[] = { m_commandList.Get() };
        m_commandQueue->ExecuteCommandLists(1, lists);
        WaitForGpu();
    }

    void LoadDDSTexture(const wchar_t* filePath, UINT descriptorIndex, ObjectRenderData& obj)
    {
        std::ifstream file(filePath, std::ios::binary);
        if (!file)
            throw std::runtime_error("Nie mozna otworzyc pliku DDS.");

        uint32_t magic = 0;
        file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        if (magic != 0x20534444)
            throw std::runtime_error("Niepoprawny plik DDS.");

        DDS_HEADER header = {};
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (header.ddspf.rgbBitCount != 32 ||
            header.ddspf.rBitMask != 0x00ff0000 ||
            header.ddspf.gBitMask != 0x0000ff00 ||
            header.ddspf.bBitMask != 0x000000ff ||
            header.ddspf.aBitMask != 0xff000000)
        {
            throw std::runtime_error("Ten przyklad obsluguje tylko DDS RGBA8.");
        }

        std::vector<uint8_t> pixelData(header.width * header.height * 4);
        file.read(reinterpret_cast<char*>(pixelData.data()), pixelData.size());

        D3D12_RESOURCE_DESC texDesc = {};
        texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texDesc.Width = header.width;
        texDesc.Height = header.height;
        texDesc.DepthOrArraySize = 1;
        texDesc.MipLevels = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        D3D12_HEAP_PROPERTIES defaultHeapProps = {};
        defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
        ThrowIfFailed(m_device->CreateCommittedResource(&defaultHeapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&obj.texture)));

        UINT64 uploadBufferSize = 0;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
        UINT numRows = 0;
        UINT64 rowSizeInBytes = 0;
        m_device->GetCopyableFootprints(&texDesc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &uploadBufferSize);

        D3D12_RESOURCE_DESC uploadDesc = {};
        uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        uploadDesc.Width = uploadBufferSize;
        uploadDesc.Height = 1;
        uploadDesc.DepthOrArraySize = 1;
        uploadDesc.MipLevels = 1;
        uploadDesc.SampleDesc.Count = 1;
        uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        D3D12_HEAP_PROPERTIES uploadHeapProps = {};
        uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        ThrowIfFailed(m_device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &uploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&obj.textureUpload)));

        uint8_t* mapped = nullptr;
        ThrowIfFailed(obj.textureUpload->Map(0, nullptr, reinterpret_cast<void**>(&mapped)));
        for (UINT y = 0; y < numRows; ++y)
        {
            memcpy(mapped + footprint.Offset + y * footprint.Footprint.RowPitch,
                   pixelData.data() + y * header.width * 4,
                   header.width * 4);
        }
        obj.textureUpload->Unmap(0, nullptr);

        ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());
        ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr));

        D3D12_TEXTURE_COPY_LOCATION dst = {};
        dst.pResource = obj.texture.Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION src = {};
        src.pResource = obj.textureUpload.Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = footprint;

        m_commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = obj.texture.Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        m_commandList->ResourceBarrier(1, &barrier);

        ThrowIfFailed(m_commandList->Close());
        ID3D12CommandList* lists[] = { m_commandList.Get() };
        m_commandQueue->ExecuteCommandLists(1, lists);
        WaitForGpu();

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
        srvHandle.ptr += static_cast<SIZE_T>(descriptorIndex) * m_srvDescriptorSize;
        m_device->CreateShaderResourceView(obj.texture.Get(), &srvDesc, srvHandle);

        obj.srvGpu = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
        obj.srvGpu.ptr += static_cast<UINT64>(descriptorIndex) * m_srvDescriptorSize;
    }

    // ----------------------------------------------------------------------------------
    // Render Loop

    void Update(float deltaTime)
    {
        // Move camera
        if (GetAsyncKeyState('Q') & 0x8000) m_cameraYaw -= m_rotationSpeed * deltaTime;
        if (GetAsyncKeyState('E') & 0x8000) m_cameraYaw += m_rotationSpeed * deltaTime;

        float sinYaw = sinf(m_cameraYaw);
        float cosYaw = cosf(m_cameraYaw);

        XMVECTOR forward = XMVectorSet(sinYaw, 0.0f, cosYaw, 0.0f);
        XMVECTOR right = XMVectorSet(cosYaw, 0.0f, -sinYaw, 0.0f);
        XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        XMVECTOR pos = XMLoadFloat3(&m_cameraPos);

        if (GetAsyncKeyState('W') & 0x8000) pos = XMVectorAdd(pos, XMVectorScale(forward, m_moveSpeed * deltaTime));
        if (GetAsyncKeyState('S') & 0x8000) pos = XMVectorSubtract(pos, XMVectorScale(forward, m_moveSpeed * deltaTime));
        if (GetAsyncKeyState('D') & 0x8000) pos = XMVectorAdd(pos, XMVectorScale(right, m_moveSpeed * deltaTime));
        if (GetAsyncKeyState('A') & 0x8000) pos = XMVectorSubtract(pos, XMVectorScale(right, m_moveSpeed * deltaTime));       
        if (GetAsyncKeyState(VK_LSHIFT) & 0x8000) pos = XMVectorAdd(pos, XMVectorScale(up, m_moveSpeed * deltaTime));       
        if (GetAsyncKeyState(VK_LCONTROL) & 0x8000) pos = XMVectorSubtract(pos, XMVectorScale(up, m_moveSpeed * deltaTime));

        XMStoreFloat3(&m_cameraPos, pos);

        XMVECTOR camTarget = XMVectorAdd(pos, forward);
        XMVECTOR camUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        XMMATRIX view = XMMatrixLookAtLH(pos, camTarget, camUp);

        XMMATRIX proj = XMMatrixPerspectiveFovLH(
            XMConvertToRadians(60.0f),
            static_cast<float>(Width) / static_cast<float>(Height),
            0.1f,
            200.0f);

        const UINT cbSize = (sizeof(ObjectConstants) + 255) & ~255u; 

        // Room
        UpdateObjectCB(0, XMMatrixIdentity(), view, proj, XMFLOAT4(0.60f, 0.60f, 0.60f, 1.0f), 6.0f, cbSize);
		// Scene base
        UpdateObjectCB(1, XMMatrixIdentity(), view, proj, XMFLOAT4(0.65f, 0.20f, 0.10f, 1.0f), 1.0f, cbSize);
        // Tables and chairs
        UpdateObjectCB(2, XMMatrixIdentity(), view, proj, XMFLOAT4(0.80f, 0.55f, 0.35f, 1.0f), 0.2f, cbSize);
		// Stairs
        UpdateObjectCB(3, XMMatrixIdentity(), view, proj, XMFLOAT4(1.0f, 0.7f, 0.5f, 1.0f), 0.1f, cbSize);
        // DJ setup
        UpdateObjectCB(4, XMMatrixIdentity(), view, proj, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), 0.1f, cbSize);
        // DJ desk
        UpdateObjectCB(5, XMMatrixIdentity(), view, proj, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), 1.0f, cbSize);
        // Speakers
        UpdateObjectCB(6, XMMatrixIdentity(), view, proj, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), 0.1f, cbSize);
    }

    void Render()
    {
        ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());
        ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), m_pipelineState.Get()));

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        m_commandList->ResourceBarrier(1, &barrier);

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
        rtvHandle.ptr += static_cast<SIZE_T>(m_frameIndex) * m_rtvDescriptorSize;

        const float clearColor[] = { 0.02f, 0.02f, 0.03f, 1.0f };
        m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

        m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

        m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
        m_commandList->RSSetViewports(1, &m_viewport);
        m_commandList->RSSetScissorRects(1, &m_scissorRect);
        m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        const UINT cbSize = (sizeof(ObjectConstants) + 255) & ~255u;
        ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
        m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);

        for (UINT i = 0; i < ObjectCount; i++)
        {
            m_commandList->IASetVertexBuffers(0, 1, &m_objects[i].vbv);
            m_commandList->IASetIndexBuffer(&m_objects[i].ibv);
            m_commandList->SetGraphicsRootConstantBufferView(0, m_constantBuffer->GetGPUVirtualAddress() + (i * cbSize));
            m_commandList->SetGraphicsRootDescriptorTable(1, m_objects[i].srvGpu);
            m_commandList->DrawIndexedInstanced(m_objects[i].indexCount, 1, 0, 0, 0);
        }

        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        m_commandList->ResourceBarrier(1, &barrier);

        ThrowIfFailed(m_commandList->Close());
        ID3D12CommandList* lists[] = { m_commandList.Get() };
        m_commandQueue->ExecuteCommandLists(1, lists);

        ThrowIfFailed(m_swapChain->Present(1, 0));
        MoveToNextFrame();
    }

    void MoveToNextFrame()
    {
        const UINT64 currentFenceValue = m_fenceValues[m_frameIndex];
        ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));

        m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

        if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex])
        {
            ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }

        m_fenceValues[m_frameIndex] = currentFenceValue + 1;
    }

    void HandleKeyboardInput(WPARAM wParam) {
        switch (wParam) {
        case VK_SPACE:
            m_lighting.ToggleAmbientLight();
            break;
        case VK_UP:
            m_lighting.AddSceneLight();
            break;
        case VK_DOWN: 
            m_lighting.RemoveSceneLight();
            break;
        case 'B':
            m_lighting.ToggleSceneLightBlur();
            break;
        case VK_OEM_PLUS:
            break;
        case VK_OEM_MINUS:
            break;
        }
    }

    void UpdateObjectCB(
        UINT index,
        const XMMATRIX& world,
        const XMMATRIX& view,
        const XMMATRIX& proj,
        const XMFLOAT4& baseColor,
        float uvScale, 
        UINT cbSize)
    {
        ObjectConstants cb{};
        XMStoreFloat4x4(&cb.world, XMMatrixTranspose(world));
        XMStoreFloat4x4(&cb.worldViewProj, XMMatrixTranspose(world * view * proj));
        cb.lightCount = m_lighting.GetLightCount();
        m_lighting.UpdateLights(cb.lights, cb.lightCount);
        cb.cameraPosition = m_cameraPos;
        cb.baseColor = baseColor;
        cb.uvScale = uvScale; 

        memcpy(m_cbvDataBegin + index * cbSize, &cb, sizeof(cb));
    }

    // ----------------------------------------------------------------------------------
    // Utils

    inline void ThrowIfFailed(HRESULT hr)
    {
        if (FAILED(hr))
            throw std::runtime_error("HRESULT failed.");
    }

    void WaitForGpu()
    {
        ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex]));
        ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
        WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
        m_fenceValues[m_frameIndex]++;
    }

    void GetHardwareAdapter(IDXGIAdapter1** ppAdapter)
    {
        *ppAdapter = nullptr;
        for (UINT adapterIndex = 0;; ++adapterIndex)
        {
            IDXGIAdapter1* adapter = nullptr;
            if (DXGI_ERROR_NOT_FOUND == m_factory->EnumAdapters1(adapterIndex, &adapter))
                break;

            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                adapter->Release();
                continue;
            }

            if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
            {
                *ppAdapter = adapter;
                return;
            }

            adapter->Release();
        }

        throw std::runtime_error("Brak odpowiedniego adaptera D3D12.");
    }
};


    // ----------------------------------------------------------------------------------
    // Entry Point

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int nCmdShow)
{
    try
    {
        Dx12App app;
        if (!app.Initialize(hInstance, nCmdShow))
            return -1;
        return app.Run();
    }
    catch (...)
    {
        MessageBoxA(nullptr, "Uruchomienie projektu nie powiodlo sie. Sprawdz Output w Visual Studio.", "Blad", MB_ICONERROR | MB_OK);
        return -1;
    }
}
