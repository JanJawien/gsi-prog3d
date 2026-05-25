#include "StructDef.h"
#include "ObjectHandler.h"
#include "LightHandler.h" 
#include "Camera.h"

#include <Audio.h>
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
        InitAudio();
        return true;
    }

    int Run()
    {
        MSG msg = {};
        LARGE_INTEGER frequency{}, prev{}, now{}, start{};
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&prev);
        start = prev;

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
                float totalTime = static_cast<float>(now.QuadPart - start.QuadPart) / static_cast<float>(frequency.QuadPart);

                Update(deltaTime, totalTime);
                Render();
            }
        }

        WaitForGpu();
        CloseHandle(m_fenceEvent);
        ShowCursor(TRUE);
        return static_cast<int>(msg.wParam);
    }

private:
    HWND m_hwnd = nullptr;

    // App params
    static const UINT FrameCount = 2;
    static const UINT Width = 1280;
    static const UINT Height = 720;
    static const UINT ObjectCount = 222;

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

    Camera m_camera;

    // Audio engine
    std::unique_ptr<AudioEngine> m_audioEngine;
    std::unique_ptr<SoundEffect> m_music;
    std::unique_ptr<SoundEffectInstance> m_musicInstance;
    AudioListener m_listener;
    AudioEmitter m_emitter;
    std::vector<int> musicTempo = { 135, 120 };
    std::vector<int> musicLightEffect = { 5, 3 };
    int m_currentMusicIndex = 0;

    bool m_djDeskGlowOn = false;
    bool m_djDeskRotateOn = false;
    float m_djDeskAngle = 0.0f;
    DirectX::XMFLOAT3 m_djDeskCenter = { 0.0f, 0.0f, 0.0f };

    // New interaction system
    int m_selectedObjectIndex = -1;

    std::array<DirectX::XMFLOAT3, ObjectCount> m_objectMoveOffset{};
    std::array<float, ObjectCount> m_objectRotationY{};
    std::array<bool, ObjectCount> m_objectVisible{};
    std::array<DirectX::XMFLOAT4, ObjectCount> m_objectBaseColor{};
    std::array<float, ObjectCount> m_objectBaseUvScale{};
    std::array<float, ObjectCount> m_objectScale{};

    // Disco floor
    bool m_discoFloorOn = true;
    float m_discoTimer = 0.0f;
    float m_discoChangeTime = 0.35f;

    UINT m_discoBaseIndex = 0;
    UINT m_discoFirstIndex = 0;
    UINT m_discoTileCount = 0;

    static const UINT DiscoRows = 8;
    static const UINT DiscoCols = 9;

    std::array<DirectX::XMFLOAT4, ObjectCount> m_discoColors{};
    LightHandler m_lighting;
    ObjectHandler m_objects;

    // GPU data
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineStateOpaque;
    ComPtr<ID3D12PipelineState> m_pipelineStateTransparent;
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
            if (app && GetForegroundWindow() == hWnd)
                app->HandleKeyboardInput(wParam);
            break;

        case WM_ACTIVATE:
            if (app && LOWORD(wParam) == WA_INACTIVE)
                app->ResetMouse();
            break;

        case WM_NCCREATE:
        {
            CREATESTRUCT* createStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));
            break;
        }
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
        ShowCursor(FALSE);
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
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.DepthStencilState.DepthEnable = TRUE;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        psoDesc.SampleDesc.Count = 1;

        // ---- OPAQUE PSO ----
        D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueDesc = psoDesc;
        opaqueDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
        opaqueDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        opaqueDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&opaqueDesc, IID_PPV_ARGS(&m_pipelineStateOpaque)));

        // ---- TRANSPARENT PSO ----
        D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentDesc = psoDesc;
        D3D12_RENDER_TARGET_BLEND_DESC blendDesc = {};
        blendDesc.BlendEnable = TRUE;
        blendDesc.LogicOpEnable = FALSE;
        blendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
        blendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
        blendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
        blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        transparentDesc.BlendState.RenderTarget[0] = blendDesc;
        transparentDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        transparentDesc.DepthStencilState.DepthEnable = TRUE;
        transparentDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&transparentDesc, IID_PPV_ARGS(&m_pipelineStateTransparent)));

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

    void InitAudio()
    {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);

        m_audioEngine = std::make_unique<AudioEngine>();
        m_music = std::make_unique<SoundEffect>(
            m_audioEngine.get(),
            L"Assets/music0.wav"
        );
        m_musicInstance = m_music->CreateInstance(
            SoundEffectInstance_Use3D
        );

        // listener = camera
        m_listener.SetPosition(XMFLOAT3(0, 0, 0));
        m_emitter.SetPosition(XMFLOAT3(8, 2, 0));
    }

    // ----------------------------------------------------------------------------------
    // Model loading functions
    XMFLOAT4 RandomDiscoColor()
    {
        std::uniform_real_distribution<float> colorDist(0.45f, 2.2f);

        return XMFLOAT4(
            colorDist(gen),
            colorDist(gen),
            colorDist(gen),
            1.0f
        );
    }

    Mesh CreateQuadMesh(float centerX, float centerY, float centerZ, float sizeX, float sizeZ)
    {
        Mesh mesh;

        float hx = sizeX * 0.5f;
        float hz = sizeZ * 0.5f;

        mesh.vertices =
        {
            { XMFLOAT3(centerX - hx, centerY, centerZ - hz), XMFLOAT2(0.0f, 0.0f) },
            { XMFLOAT3(centerX + hx, centerY, centerZ - hz), XMFLOAT2(1.0f, 0.0f) },
            { XMFLOAT3(centerX + hx, centerY, centerZ + hz), XMFLOAT2(1.0f, 1.0f) },
            { XMFLOAT3(centerX - hx, centerY, centerZ + hz), XMFLOAT2(0.0f, 1.0f) }
        };

        mesh.indices =
        {
            0, 1, 2,
            0, 2, 3
        };

        return mesh;
    }

    void CreateSolidColorTexture(UINT descriptorIndex, ObjectRenderData& obj, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
    {
        uint8_t pixelData[4] = { r, g, b, a };

        D3D12_RESOURCE_DESC texDesc = {};
        texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texDesc.Width = 1;
        texDesc.Height = 1;
        texDesc.DepthOrArraySize = 1;
        texDesc.MipLevels = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        D3D12_HEAP_PROPERTIES defaultHeapProps = {};
        defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        ThrowIfFailed(m_device->CreateCommittedResource(
            &defaultHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &texDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&obj.texture)));

        UINT64 uploadBufferSize = 0;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
        UINT numRows = 0;
        UINT64 rowSizeInBytes = 0;

        m_device->GetCopyableFootprints(
            &texDesc,
            0,
            1,
            0,
            &footprint,
            &numRows,
            &rowSizeInBytes,
            &uploadBufferSize
        );

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

        ThrowIfFailed(m_device->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &uploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&obj.textureUpload)));

        uint8_t* mapped = nullptr;
        ThrowIfFailed(obj.textureUpload->Map(0, nullptr, reinterpret_cast<void**>(&mapped)));
        memcpy(mapped + footprint.Offset, pixelData, 4);
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

    void AddDiscoBase(float centerX, float y, float centerZ, float sizeX, float sizeZ)
    {
        UINT index = static_cast<UINT>(m_objects.GetObjects().size());

        if (index >= ObjectCount)
            return;

        ObjectRenderData obj{};
        obj.mesh = CreateQuadMesh(centerX, y, centerZ, sizeX, sizeZ);
        obj.meshCenter = XMFLOAT3(centerX, y, centerZ);

        XMStoreFloat4x4(&obj.worldMatrix, XMMatrixIdentity());

        CreateMeshBuffers(obj);
        CreateSolidColorTexture(index, obj, 255, 255, 255, 255);

        m_objects.GetObjects().push_back(obj);

        m_objectMoveOffset[index] = XMFLOAT3(0.0f, 0.0f, 0.0f);
        m_objectRotationY[index] = 0.0f;
        m_objectVisible[index] = true;
        m_objectBaseColor[index] = XMFLOAT4(0.05f, 0.05f, 0.05f, 1.0f);
        m_objectBaseUvScale[index] = 1.0f;
        m_objectScale[index] = 1.0f;
        m_discoBaseIndex = index;
    }

    void AddDiscoTile(float x, float y, float z, float size)
    {
        UINT index = static_cast<UINT>(m_objects.GetObjects().size());

        if (index >= ObjectCount)
            return;

        ObjectRenderData obj{};
        obj.mesh = CreateQuadMesh(x, y, z, size, size);
        obj.meshCenter = XMFLOAT3(x, y, z);
        obj.isTransparent = false;
        obj.isLightCone = false;
        obj.isVisible = true;

        XMStoreFloat4x4(&obj.worldMatrix, XMMatrixIdentity());

        CreateMeshBuffers(obj);

        // Bia³a tekstura 1x1, ¿eby nie by³o wzorków ani czarnych pasków
        CreateSolidColorTexture(index, obj, 255, 255, 255, 255);

        m_objects.GetObjects().push_back(obj);

        m_objectMoveOffset[index] = XMFLOAT3(0.0f, 0.0f, 0.0f);
        m_objectRotationY[index] = 0.0f;
        m_objectVisible[index] = true;
        m_objectBaseColor[index] = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        m_objectBaseUvScale[index] = 1.0f;
        m_objectScale[index] = 1.0f;
        m_discoColors[index] = RandomDiscoColor();
    }

    void CreateDiscoFloor()
    {
        m_discoFirstIndex = 0;
        m_discoTileCount = 0;

        float tileSize = 0.62f;
        float gap = 0.07f;
        float step = tileSize + gap;

        // Parkiet lekko mniejszy i bardziej na środku sali.
        // X idzie od sceny w stronę kamery, Z to szerokość sali.
        float startX = 0.35f;

        float totalX = DiscoRows * tileSize + (DiscoRows - 1) * gap;
        float totalZ = DiscoCols * tileSize + (DiscoCols - 1) * gap;

        float startZ = -0.5f * totalZ + tileSize * 0.5f;

        float baseCenterX = startX + 0.5f * (totalX - tileSize);
        float baseCenterZ = startZ + 0.5f * (totalZ - tileSize);

        AddDiscoBase(
            baseCenterX,
            0.020f,
            baseCenterZ,
            totalX + 0.35f,
            totalZ + 0.35f
        );

        m_discoFirstIndex = static_cast<UINT>(m_objects.GetObjects().size());

        float tilesY = 0.035f;

        for (UINT row = 0; row < DiscoRows; ++row)
        {
            for (UINT col = 0; col < DiscoCols; ++col)
            {
                float x = startX + static_cast<float>(row) * step;
                float z = startZ + static_cast<float>(col) * step;

                AddDiscoTile(x, tilesY, z, tileSize);
            }
        }

        m_discoTileCount =
            static_cast<UINT>(m_objects.GetObjects().size()) - m_discoFirstIndex;
    }

    void UpdateDiscoFloor(float deltaTime)
    {
        if (!m_discoFloorOn)
            return;

        m_discoTimer += deltaTime;

        if (m_discoTimer < m_discoChangeTime)
            return;

        m_discoTimer = 0.0f;

        for (UINT i = m_discoFirstIndex; i < m_discoFirstIndex + m_discoTileCount; ++i)
        {
            if (i < ObjectCount)
                m_discoColors[i] = RandomDiscoColor();
        }
    }
    
    void LoadModels()
    {
        m_objects.SetMeshBufferFunc([this](ObjectRenderData& obj)
            {
                CreateMeshBuffers(obj);
            });

        m_objects.SetTextureFunc([this](const wchar_t* path, UINT i, ObjectRenderData& obj)
            {
                LoadDDSTexture(path, i, obj);
            });

        m_objects.LoadAllObjects();

        for (UINT i = 0; i < ObjectCount; ++i)
        {
            m_objectMoveOffset[i] = XMFLOAT3(0.0f, 0.0f, 0.0f);
            m_objectRotationY[i] = 0.0f;
            m_objectVisible[i] = true;
            m_objectBaseColor[i] = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
            m_objectBaseUvScale[i] = 1.0f;
            m_objectScale[i] = 1.0f;
            m_discoColors[i] = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        }

        for (auto& obj : m_objects.GetObjects())
        {
            XMStoreFloat4x4(&obj.worldMatrix, XMMatrixIdentity());
            obj.isVisible = true;
        }

        // Ukrywamy stary, zbiorczy model stolików i krzeseł.
        // Nie usuwamy go z ObjectHandler.cpp, bo wtedy przesunęłyby się indeksy obiektów.
        if (m_objects.GetObjects().size() > 2)
        {
            m_objectVisible[2] = false;
            m_objects.GetObjects()[2].isVisible = false;
        }

        m_djDeskCenter = m_objects.GetObjects()[5].meshCenter;

        // Potem tworzymy disco floor, żeby był przed sceną i na środku
        CreateDiscoFloor();
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

        if (header.ddspf.rgbBitCount != 32)
        {
            throw std::runtime_error("Ten przyklad obsluguje tylko 32-bitowe pliki DDS.");
        }

        DXGI_FORMAT textureFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

        if (header.ddspf.rBitMask == 0x00ff0000)
        {
            textureFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
        }
        else if (header.ddspf.rBitMask == 0x000000ff)
        {
            textureFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        }

        std::vector<uint8_t> pixelData(header.width * header.height * 4);
        file.read(reinterpret_cast<char*>(pixelData.data()), pixelData.size());

        D3D12_RESOURCE_DESC texDesc = {};
        texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texDesc.Width = header.width;
        texDesc.Height = header.height;
        texDesc.DepthOrArraySize = 1;
        texDesc.MipLevels = 1;
        texDesc.Format = textureFormat; 
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
        srvDesc.Format = textureFormat; 
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

    void Update(float deltaTime, float totalTime)
    {
        // Move camera
        m_camera.Update(deltaTime, m_hwnd);
        XMMATRIX view = m_camera.GetViewMatrix();

        m_lighting.UpdateSpotlights(totalTime);
        UpdateDiscoFloor(deltaTime);

        XMMATRIX proj = XMMatrixPerspectiveFovLH(
            XMConvertToRadians(60.0f),
            static_cast<float>(Width) / static_cast<float>(Height),
            0.1f,
            200.0f);

        // Update audio
        m_audioEngine->Update();
        m_listener.SetPosition(m_camera.GetPosition());
        m_musicInstance->Apply3D(m_listener, m_emitter);

        const UINT cbSize = (sizeof(ObjectConstants) + 255) & ~255u; 
        
        // Room
        UpdateObjectCB(0, XMMatrixIdentity(), view, proj, XMFLOAT4(0.60f, 0.60f, 0.60f, 1.0f), 6.0f, cbSize);
		// Scene base
        UpdateObjectCB(1, XMMatrixIdentity(), view, proj, XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f), 7.0f, cbSize);
        // Tables and chairs
        UpdateObjectCB(2, XMMatrixIdentity(), view, proj, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), 0.2f, cbSize);
		// Stairs
        UpdateObjectCB(3, XMMatrixIdentity(), view, proj, XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f), 0.1f, cbSize);
        // DJ setup
        UpdateObjectCB(4, XMMatrixIdentity(), view, proj, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), 0.1f, cbSize);
        // DJ desk
        UpdateObjectCB(5, XMMatrixIdentity(), view, proj, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), 1.0f, cbSize);
        // Speakers
        UpdateObjectCB(6, XMMatrixIdentity(), view, proj, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), 0.1f, cbSize);
        UpdateObjectCB(7, XMMatrixIdentity(), view, proj, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), 0.1f, cbSize);
        UpdateObjectCB(8, XMMatrixIdentity(), view, proj, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), 0.1f, cbSize);
        UpdateObjectCB(9, XMMatrixIdentity(), view, proj, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), 0.1f, cbSize);
        // Railing
        UpdateObjectCB(10, XMMatrixIdentity(), view, proj, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), 1.0f, cbSize);
        UpdateObjectCB(11, XMMatrixIdentity(), view, proj, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), 1.0f, cbSize);
        UpdateObjectCB(12, XMMatrixIdentity(), view, proj, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), 1.0f, cbSize);
        // lightCone
        for (int i = 0; i < MAX_LIGHTS_SCENE; ++i) {
            XMMATRIX coneWorld = m_lighting.GetSpotlightRot(i) * m_lighting.GetSpotlightPos(i);
            UpdateObjectCB(13 + i, coneWorld, view, proj, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), 1.0f, cbSize);
            UpdateObjectCB(22 + i, coneWorld, view, proj, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), 1.0f, cbSize);
        }
//
        UpdateObjectCB(32, XMMatrixTranslation(0.0f, 0.0f, 1.0f), view, proj, XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f), 0.5f, cbSize);
//
        UpdateObjectCB(37, XMMatrixIdentity(), view, proj, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), 1.0f, cbSize);
//
        auto updateTableGroup = [&](UINT firstIndex, UINT count, float x, float z, float rotationDegrees)
            {
                XMMATRIX tableT = XMMatrixTranslation(x, 0.0f, z);
                XMMATRIX tableR = XMMatrixRotationY(XMConvertToRadians(rotationDegrees));
                XMMATRIX tableWorld = tableR * tableT;

                for (UINT j = 0; j < count; ++j)
                {
                    UINT objIndex = firstIndex + j;

                    UpdateObjectCB(
                        objIndex,
                        tableWorld,
                        view,
                        proj,
                        XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
                        1.0f,
                        cbSize
                    );
                }
            };

        // LEWA STRONA
        updateTableGroup(38, 5, -4.0f, -3.4f, -90.0f);
        updateTableGroup(43, 3, -6.6f, -3.4f, -90.0f);

        // PRAWA STRONA
        updateTableGroup(46, 4, -4.0f, 3.4f, 90.0f);
        updateTableGroup(50, 3, -6.6f, 3.4f, 90.0f);

        // TYLNA ŚCIANA / okolice drzwi
        updateTableGroup(53, 3, -7.9f, -0.8f, 0.0f);

        // Dynamic duplicated objects reuse existing mesh/texture data but get their own transform and constant buffer.
        for (UINT i = 56; i < m_objects.GetObjects().size() && i < ObjectCount; ++i)
        {
            XMFLOAT4 color = m_objectBaseColor[i];
            float uvScale = m_objectBaseUvScale[i];

            // Czarna p³yta pod parkietem
            if (i == m_discoBaseIndex)
            {
                color = m_discoFloorOn
                    ? XMFLOAT4(0.05f, 0.05f, 0.05f, 1.0f)
                    : XMFLOAT4(0.03f, 0.03f, 0.03f, 1.0f);

                uvScale = 1.0f;
            }
            // Kolorowe kafelki parkietu
            else if (i >= m_discoFirstIndex && i < m_discoFirstIndex + m_discoTileCount)
            {
                color = m_discoFloorOn
                    ? m_discoColors[i]
                    : XMFLOAT4(0.10f, 0.10f, 0.10f, 1.0f);

                uvScale = 1.0f;
            }

            UpdateObjectCB(
                i,
                XMMatrixIdentity(),
                view,
                proj,
                color,
                uvScale,
                cbSize
            );
        }
    }

    void Render()
    {
        ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());
        ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr));

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

        // Opaque pass
        int i = -1;
        m_commandList->SetPipelineState(m_pipelineStateOpaque.Get());

        for (auto& obj : m_objects.GetObjects())
        {
            ++i;

            if (i >= static_cast<int>(ObjectCount))
                continue;

            if (!m_objectVisible[i] || !obj.isVisible)
                continue;

            if (obj.isTransparent)
                continue;

            m_commandList->IASetVertexBuffers(0, 1, &obj.vbv);
            m_commandList->IASetIndexBuffer(&obj.ibv);

            m_commandList->SetGraphicsRootConstantBufferView(
                0,
                m_constantBuffer->GetGPUVirtualAddress() + (i * cbSize)
            );

            m_commandList->SetGraphicsRootDescriptorTable(1, obj.srvGpu);
            m_commandList->DrawIndexedInstanced(obj.indexCount, 1, 0, 0, 0);
        }

        // Transparent pass
        i = -1;
        m_commandList->SetPipelineState(m_pipelineStateTransparent.Get());

        for (auto& obj : m_objects.GetObjects())
        {
            ++i;

            if (i >= static_cast<int>(ObjectCount))
                continue;

            if (!m_objectVisible[i] || !obj.isVisible)
                continue;

            if (!obj.isTransparent)
                continue;

            m_commandList->IASetVertexBuffers(0, 1, &obj.vbv);
            m_commandList->IASetIndexBuffer(&obj.ibv);

            m_commandList->SetGraphicsRootConstantBufferView(
                0,
                m_constantBuffer->GetGPUVirtualAddress() + (i * cbSize)
            );

            m_commandList->SetGraphicsRootDescriptorTable(1, obj.srvGpu);
            m_commandList->DrawIndexedInstanced(obj.indexCount, 1, 0, 0, 0);
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

    void HandleKeyboardInput(WPARAM wParam)
    {
        const float moveStep = 0.25f;
        const float rotateStep = XMConvertToRadians(15.0f);

        auto hasSelectedObject = [this]() -> bool
            {
                return m_selectedObjectIndex >= 0 &&
                    m_selectedObjectIndex < static_cast<int>(ObjectCount) &&
                    m_selectedObjectIndex < static_cast<int>(m_objects.GetObjects().size()) &&
                    m_objectVisible[m_selectedObjectIndex];
            };

        auto updateWindowTitle = [this]()
            {
                std::wstring title =
                    L"Selected: " + std::to_wstring(m_selectedObjectIndex) +
                    L" | E select/deselect | IJKL/UO move | R rotate | M duplicate | DEL hide";

                SetWindowText(m_hwnd, title.c_str());
            };

        auto duplicateSelectedObject = [this, &updateWindowTitle]()
            {
                if (m_selectedObjectIndex < 0)
                    return;

                if (m_selectedObjectIndex >= static_cast<int>(m_objects.GetObjects().size()))
                    return;

                if (m_objects.GetObjects().size() >= ObjectCount)
                    return;

                UINT newIndex = static_cast<UINT>(m_objects.GetObjects().size());

                ObjectRenderData copy = m_objects.GetObjects()[m_selectedObjectIndex];
                copy.isVisible = true;
                XMStoreFloat4x4(&copy.worldMatrix, XMMatrixIdentity());

                m_objects.GetObjects().push_back(copy);

                m_objectVisible[newIndex] = true;
                m_objects.GetObjects()[newIndex].isVisible = true;

                m_objectMoveOffset[newIndex] = m_objectMoveOffset[m_selectedObjectIndex];
                m_objectMoveOffset[newIndex].x += 0.5f;
                m_objectMoveOffset[newIndex].z += 0.5f;

                m_objectRotationY[newIndex] = m_objectRotationY[m_selectedObjectIndex];

                m_objectBaseColor[newIndex] = m_objectBaseColor[m_selectedObjectIndex];
                m_objectBaseUvScale[newIndex] = m_objectBaseUvScale[m_selectedObjectIndex];
                m_objectScale[newIndex] = m_objectScale[m_selectedObjectIndex];

                m_selectedObjectIndex = static_cast<int>(newIndex);
                updateWindowTitle();
            };

        auto changeMusic = [this](int index) {
                if (index < 0) return;
                if (index >= musicTempo.size()) return;
                m_currentMusicIndex = index;
                auto name = L"Assets/music" + std::to_wstring(index) + L".wav";

                m_musicInstance->Stop();
                m_audioEngine->Update();
                m_musicInstance.reset();
                m_music.reset();

                m_music = std::make_unique<SoundEffect>(
                    m_audioEngine.get(),
                    name.c_str()
                );
                m_musicInstance = m_music->CreateInstance(
                    SoundEffectInstance_Use3D
                );
                m_musicInstance->Play();

                m_lighting.SetTempo(musicTempo[index]);
                m_lighting.ChangeLightEffect(musicLightEffect[index]);
            };

        switch (wParam)
        {
        case VK_SPACE:
            m_lighting.ToggleAmbientLight();
            break;

        case VK_UP:
            m_lighting.AddSceneLight();
            break;

        case VK_DOWN:
            m_lighting.RemoveSceneLight();
            break;

        case VK_LEFT:
            // if laptop selected, prev song
            if (m_selectedObjectIndex == 4)
                changeMusic(m_currentMusicIndex - 1);
            break;

        case VK_RIGHT:
            if (m_selectedObjectIndex == 4)
                changeMusic(m_currentMusicIndex + 1);
            break;

        case 'B':
            m_lighting.ToggleSceneLightBlur();
            break;

        case VK_ESCAPE:
            m_musicInstance->Stop();
            m_audioEngine->Update();
            m_musicInstance.reset();
            m_music.reset();
            m_audioEngine->Suspend();
            m_audioEngine.reset();
            CoUninitialize();
            
            PostQuitMessage(0);
            break;

        // Select / deselect object
        case 'E':
        {
            int clickedObject = m_objects.GetClickedObjectIndex(
                m_camera.GetPosition(),
                m_camera.GetForward()
            );

            // Dont select room model
            if (clickedObject == 0) break;

            if (clickedObject == m_selectedObjectIndex)
            {
                m_selectedObjectIndex = -1;
            }
            else if (
                clickedObject >= 0 &&
                clickedObject < static_cast<int>(ObjectCount) &&
                clickedObject < static_cast<int>(m_objects.GetObjects().size()) &&
                m_objectVisible[clickedObject] &&
                !m_objects.GetObjects()[clickedObject].isLightCone)
            {
                m_selectedObjectIndex = clickedObject;
            }
            else
            {
                m_selectedObjectIndex = -1;
            }

            updateWindowTitle();
            break;
        }

        // Unselect object manually
        case 'X':
            m_selectedObjectIndex = -1;
            updateWindowTitle();
            break;

            // Move selected object
        case 'I':
            if (hasSelectedObject())
                m_objectMoveOffset[m_selectedObjectIndex].z += moveStep;
            break;

        case 'K':
            if (hasSelectedObject())
                m_objectMoveOffset[m_selectedObjectIndex].z -= moveStep;
            break;

        case 'J':
            if (hasSelectedObject())
                m_objectMoveOffset[m_selectedObjectIndex].x -= moveStep;
            break;

        case 'L':
            if (hasSelectedObject())
                m_objectMoveOffset[m_selectedObjectIndex].x += moveStep;
            break;

        case 'U':
            if (hasSelectedObject())
                m_objectMoveOffset[m_selectedObjectIndex].y += moveStep;
            break;

        case 'O':
            if (hasSelectedObject())
                m_objectMoveOffset[m_selectedObjectIndex].y -= moveStep;
            break;

            // Rotate selected object
        case 'R':
            if (hasSelectedObject())
                m_objectRotationY[m_selectedObjectIndex] += rotateStep;
            break;

            // Duplicate selected object
        case 'M':
            if (hasSelectedObject())
                duplicateSelectedObject();
            break;

            // Hide selected object
        case VK_DELETE:
            if (hasSelectedObject())
            {
                m_objectVisible[m_selectedObjectIndex] = false;
                m_objects.GetObjects()[m_selectedObjectIndex].isVisible = false;
                m_selectedObjectIndex = -1;
                updateWindowTitle();
            }
            break;

        case VK_OEM_COMMA:
            m_lighting.ChangeLightEffectPrev();
            break;

        case VK_OEM_PERIOD:
            m_lighting.ChangeLightEffectNext();
            break;

        case '1':
            m_lighting.ChangeLightEffect(1);
            break;

        case '2':
            m_lighting.ChangeLightEffect(2);
            break;

        case '3':
            m_discoFloorOn = !m_discoFloorOn;
            break;

        case '0':
            m_lighting.ChangeLightEffect(0);
            break;
        }
    }

    void ResetMouse()
    {
        m_camera.ResetMouse();
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

        XMMATRIX finalWorld = world;

        if (index < ObjectCount && index < m_objects.GetObjects().size())
        {
            m_objectBaseColor[index] = baseColor;
            m_objectBaseUvScale[index] = uvScale;

            XMFLOAT3 offset = m_objectMoveOffset[index];
            float rotY = m_objectRotationY[index];
            float scale = m_objectScale[index];

            XMFLOAT3 center = m_objects.GetObjects()[index].meshCenter;

            XMMATRIX transformAroundCenter =
                XMMatrixTranslation(-center.x, -center.y, -center.z) *
                XMMatrixScaling(scale, scale, scale) *
                XMMatrixRotationY(rotY) *
                XMMatrixTranslation(center.x, center.y, center.z);

            XMMATRIX move =
                XMMatrixTranslation(offset.x, offset.y, offset.z);

            finalWorld = transformAroundCenter * world * move;

            XMStoreFloat4x4(&m_objects.GetObjects()[index].worldMatrix, finalWorld);
            m_objects.GetObjects()[index].isVisible = m_objectVisible[index];
        }

        XMFLOAT4 finalColor = baseColor;

        // Selected object highlight
        if (static_cast<int>(index) == m_selectedObjectIndex)
        {
            finalColor = XMFLOAT4(
                finalColor.x * 1.5f,
                finalColor.y * 1.2f,
                finalColor.z * 0.6f,
                finalColor.w
            );
        }

        XMStoreFloat4x4(&cb.world, XMMatrixTranspose(finalWorld));
        XMStoreFloat4x4(&cb.worldViewProj, XMMatrixTranspose(finalWorld * view * proj));

        cb.lightCount = m_lighting.GetLightCount();
        m_lighting.UpdateLights(cb.lights, cb.lightCount);

        cb.cameraPosition = m_camera.GetPosition();
        cb.baseColor = finalColor;
        cb.uvScale = uvScale;
        cb.isLightCone = m_objects.GetObjects()[index].isLightCone;

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
