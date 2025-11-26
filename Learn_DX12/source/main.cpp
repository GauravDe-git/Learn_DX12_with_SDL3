
// --- DirectX 12 specific headers ---
#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <wrl.h> // <-- For ComPtr (smart pointers)

#include <DirectXMath.h>

// --- D3d12 Extension Library ---
#include <d3dx12.h>

// --- Windows / C++ Conflict Fix ---
// Fixes conflicts with Windows.h macros
#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

#include <iostream>

// --- Helpers ---
#include "../include/helpers.hpp"
#include "../include/CommandQueue.hpp"

#include "../include/GameCore.hpp"

// --- Namespaces ---
using namespace Microsoft::WRL; // For ComPtr
using namespace DirectX;    //temporariliy put the directx namespace here for the dxmath

// --- Vertex Structure ---
struct VertexPosColor
{
    XMFLOAT3 Position;
    XMFLOAT3 Color;
};

// --- Cube Data ---
static VertexPosColor g_Vertices[8] = {
    { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) }, // 0
    { XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) }, // 1
    { XMFLOAT3(1.0f,  1.0f, -1.0f), XMFLOAT3(1.0f, 1.0f, 0.0f) }, // 2
    { XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) }, // 3
    { XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) }, // 4
    { XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT3(0.0f, 1.0f, 1.0f) }, // 5
    { XMFLOAT3(1.0f,  1.0f,  1.0f), XMFLOAT3(1.0f, 1.0f, 1.0f) }, // 6
    { XMFLOAT3(1.0f, -1.0f,  1.0f), XMFLOAT3(1.0f, 0.0f, 1.0f) }  // 7
};

static WORD g_Indicies[36] =
{
    0, 1, 2, 0, 2, 3,
    4, 6, 5, 4, 7, 6,
    4, 5, 1, 4, 1, 0,
    3, 2, 6, 3, 6, 7,
    1, 5, 6, 1, 6, 2,
    4, 0, 3, 4, 3, 7
};

// ----------- The Application Class -----------
class HD2D_Renderer : public Game
{
public:
    HD2D_Renderer()
        : Game{L"Learn DX12 - HD 2D renderer", 1280, 720, true}
        , m_CommandQueue{D3D12_COMMAND_LIST_TYPE_DIRECT}
        , m_TearingSupported{CheckTearingSupport()}
        , m_UseWarp{false}
        , m_ScissorRect{CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX)}
        , m_Viewport{CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(1280), static_cast<float>(720))}
        , m_FoV{45.0f} 
        , m_ContentLoaded{false}
    {
    }

    virtual void Startup() override
    {
        // 1. Enable Debug Layer
#if defined(_DEBUG)
        ComPtr<ID3D12Debug> debugInterface;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface))))
        {
            debugInterface->EnableDebugLayer();
        }
#endif

        // 2. Create Device
        ComPtr<IDXGIAdapter4> adapter = GetAdapter(m_UseWarp); // can fall back to software rasterizer?
        ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_Device)));

        // 3. Create Command Queue
        m_CommandQueue.Create(m_Device.Get());

        // 4. Create Swap Chain
        // Need to get the HWND from GameCore's SDL Window
        HWND hwnd = (HWND)SDL_GetPointerProperty(
            SDL_GetWindowProperties(GameCore::g_Window),
            SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);

        m_SwapChain = CreateSwapChain(hwnd, m_CommandQueue, m_Width, m_Height, m_NumFrames);

        // 5. Create Descriptor Heaps & RTVs
        m_RTVDescriptorHeap = CreateDescriptorHeap(m_Device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, m_NumFrames);
        m_RTVDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        UpdateRenderTargetViews(m_Device, m_SwapChain, m_RTVDescriptorHeap);

        // 6. Create Command List
        ID3D12CommandAllocator* allocator = m_CommandQueue.RequestAllocator();
        ThrowIfFailed(m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, nullptr, IID_PPV_ARGS(&m_CommandList)));
        ThrowIfFailed(m_CommandList->Close());
        m_CommandQueue.DiscardAllocator(0, allocator);

        // 7. Finish Setup
        m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();
        m_IsInitialized = true;

        // ======== Load Content ============= //
        
        // A. Create a Command List for Uploading Data
        // Later on in the engine, might have to use a separate Copy Queue, but Direct Queue works fine for now.
        // reuse m_CommandList which was closed in step 6.
        ThrowIfFailed(m_CommandList->Reset(m_CommandQueue.RequestAllocator(), nullptr));

        // B. Upload Vertex Buffer
        ComPtr<ID3D12Resource> intermediateVertexBuffer;
        UpdateBufferResource(m_CommandList.Get(),
            &m_VertexBuffer, &intermediateVertexBuffer,
            _countof(g_Vertices), sizeof(VertexPosColor), g_Vertices);

        // Create the Vertex Buffer View
        m_VertexBufferView.BufferLocation = m_VertexBuffer->GetGPUVirtualAddress();
        m_VertexBufferView.SizeInBytes = sizeof(g_Vertices);
        m_VertexBufferView.StrideInBytes = sizeof(VertexPosColor);

        // C. Upload Index Buffer
        ComPtr<ID3D12Resource> intermediateIndexBuffer;
        UpdateBufferResource(m_CommandList.Get(),
            &m_IndexBuffer, &intermediateIndexBuffer,
            _countof(g_Indicies), sizeof(WORD), g_Indicies);

        // Create the Index Buffer View
        m_IndexBufferView.BufferLocation = m_IndexBuffer->GetGPUVirtualAddress();
        m_IndexBufferView.Format = DXGI_FORMAT_R16_UINT;
        m_IndexBufferView.SizeInBytes = sizeof(g_Indicies);

        // D. Create DSV Descriptor Heap
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_Device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_DSVHeap)));

        // E. Load Shaders
        // (This expects VertexShader.cso and PixelShader.cso to be in the same folder as the executable.)
        ComPtr<ID3DBlob> vertexShaderBlob;
        ThrowIfFailed(D3DReadFileToBlob(L"VertexShader.cso", &vertexShaderBlob));

        ComPtr<ID3DBlob> pixelShaderBlob;
        ThrowIfFailed(D3DReadFileToBlob(L"PixelShader.cso", &pixelShaderBlob));

        // F. Create Input Layout
        // This matches the "VertexPosColor" struct in our HLSL
        D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        // G. Create Root Signature

        // 1. Check Feature Support
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
        if (FAILED(m_Device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
        {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        // 2. Define Flags
        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

        // 3. Define Root Parameters (1 Constant Buffer for MVP Matrix)
        CD3DX12_ROOT_PARAMETER1 rootParameters[1];
        // 16 floats (matrix) / 4 bytes = size in 32-bit values
        // Register b0, Space 0, Visible to Vertex Shader
        rootParameters[0].InitAsConstants(sizeof(XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

        // 4. Create Description
        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
        rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

        // 5. Serialize and Create
        ComPtr<ID3DBlob> rootSignatureBlob;
        ComPtr<ID3DBlob> errorBlob;
        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDescription,
            featureData.HighestVersion, &rootSignatureBlob, &errorBlob));

        ThrowIfFailed(m_Device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
            rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_RootSignature)));

        // H. Create Pipeline State Object (PSO)

        // 1. Define the Pipeline State Stream Structure
        // (This struct matches the layout expected by D3D12 for a stream)
        struct PipelineStateStream
        {
            CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
            CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
            CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
            CD3DX12_PIPELINE_STATE_STREAM_VS VS;
            CD3DX12_PIPELINE_STATE_STREAM_PS PS;
            CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
            CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
        } pipelineStateStream;

        // 2. Define RTV Formats
        D3D12_RT_FORMAT_ARRAY rtvFormats = {};
        rtvFormats.NumRenderTargets = 1;
        rtvFormats.RTFormats[0] = DXGI_FORMAT_R10G10B10A2_UNORM; // Matches SwapChain format

        // 3. Fill the Stream
        pipelineStateStream.pRootSignature = m_RootSignature.Get();
        pipelineStateStream.InputLayout = { inputLayout, _countof(inputLayout) };
        pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
        pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
        pipelineStateStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        pipelineStateStream.RTVFormats = rtvFormats;

        // 4. Create the PSO
        D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
            sizeof(PipelineStateStream), &pipelineStateStream
        };
        ThrowIfFailed(m_Device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_PipelineState)));

        // I. Create Depth Buffer
        ResizeDepthBuffer(m_Width, m_Height);

        m_ContentLoaded = true;

        // ExecuteCommandList: to close it automatically
        m_CommandQueue.ExecuteCommandList(m_CommandList.Get());

        // Wait for the upload to finish before we delete the intermediate buffers
        // (The ComPtrs 'intermediateVertexBuffer' and 'intermediateIndexBuffer' will be destroyed
        // at the end of this function, so the GPU must be done with them by then.)
        m_CommandQueue.WaitForIdle();
    }

    virtual void Cleanup() override
    {
        m_CommandQueue.WaitForIdle();
        m_CommandQueue.Shutdown();
    }

    virtual void Update(float deltaTime) override
    {
        // 1. FPS Calculation 
        static double elapsedSeconds = 0.0;
        static uint64_t frameCounter = 0;
        static double totalTime = 0.0; // Track total time for rotation

        elapsedSeconds += deltaTime;
        totalTime += deltaTime;
        frameCounter++;

        if (elapsedSeconds > 1.0)
        {
            char buffer[500];
            auto fps = frameCounter / elapsedSeconds;
            sprintf_s(buffer, 500, "FPS: %f\n", fps);
            OutputDebugStringA(buffer);
            frameCounter = 0;
            elapsedSeconds = 0.0;
        }

        // 2. Update Model Matrix (Rotate cube)
        float angle = static_cast<float>(totalTime * 90.0);
        const XMVECTOR rotationAxis = XMVectorSet(0, 1, 1, 0);
        m_ModelMatrix = XMMatrixRotationAxis(rotationAxis, XMConvertToRadians(angle));

        // 3. Update View Matrix (Camera position)
        const XMVECTOR eyePosition = XMVectorSet(0, 0, -10, 1);
        const XMVECTOR focusPoint = XMVectorSet(0, 0, 0, 1);
        const XMVECTOR upDirection = XMVectorSet(0, 1, 0, 0);
        m_ViewMatrix = XMMatrixLookAtLH(eyePosition, focusPoint, upDirection);

        // 4. Update Projection Matrix (Perspective)
        float aspectRatio = static_cast<float>(m_Width) / static_cast<float>(m_Height);
        m_ProjectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(m_FoV), aspectRatio, 0.1f, 100.0f);
    }

    virtual void Render() override
    {
        if (!m_IsInitialized) return;

        // 1. Request an Allocator 
        ID3D12CommandAllocator* allocator = m_CommandQueue.RequestAllocator();

        // 2. Wait for Previous Frame
        // (Wait for the GPU to finish with the command allocator for this frame index)
        m_CommandQueue.WaitForFence(m_FrameFenceValues[m_CurrentBackBufferIndex]);

        // 3. Reset command List
        m_CommandList->Reset(allocator, m_PipelineState.Get()); // <--- Bind PSO here!

        auto backBuffer = m_BackBuffers[m_CurrentBackBufferIndex];
        auto rtv = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
            m_CurrentBackBufferIndex, m_RTVDescriptorSize);
        auto dsv = m_DSVHeap->GetCPUDescriptorHandleForHeapStart();

        // -- RECORD COMMANDS -- //

        // A. Transition to Render Target
        TransitionResource(m_CommandList, backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

        // B. Clear RTV & DSV
        FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };
        ClearRTV(m_CommandList, rtv, clearColor);
        ClearDepth(m_CommandList, dsv);

        // C. Set Root Signature
        m_CommandList->SetGraphicsRootSignature(m_RootSignature.Get());

        // D. Setup Input Assembler
        m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_CommandList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
        m_CommandList->IASetIndexBuffer(&m_IndexBufferView);

        // E. Setup Rasterizer State
        m_CommandList->RSSetViewports(1, &m_Viewport);
        m_CommandList->RSSetScissorRects(1, &m_ScissorRect);

        // F. Bind Render Targets
        m_CommandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

        // G. Update Root Parameters (MVP Matrix)
        XMMATRIX mvpMatrix = XMMatrixMultiply(m_ModelMatrix, m_ViewMatrix);
        mvpMatrix = XMMatrixMultiply(mvpMatrix, m_ProjectionMatrix);
        m_CommandList->SetGraphicsRoot32BitConstants(0, sizeof(XMMATRIX) / 4, &mvpMatrix, 0);

        // H. Draw
        m_CommandList->DrawIndexedInstanced(_countof(g_Indicies), 1, 0, 0, 0);

        // I. Transition to Present
        TransitionResource(m_CommandList, backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

        // Execute (CommandQueue closes the list automatically)
        uint64_t fenceValue = m_CommandQueue.ExecuteCommandList(m_CommandList.Get());

        // Present
        UINT syncInterval = m_VSync ? 1 : 0;
        UINT presentFlags = (m_TearingSupported && !m_VSync) ? DXGI_PRESENT_ALLOW_TEARING : 0;
        m_SwapChain->Present(syncInterval, presentFlags);

        // Cleanup
        m_CommandQueue.DiscardAllocator(fenceValue, allocator);
        m_FrameFenceValues[m_CurrentBackBufferIndex] = fenceValue;
        m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();
    }

    virtual void OnKeyDown(SDL_Keycode key) override
    {
        if (key == SDLK_F11)
        {
            GameCore::ToggleFullscreen();
        }
        if (key == SDLK_V)
        {
            m_VSync = !m_VSync;
            std::cout << "VSync: " << (m_VSync ? "On" : "Off") << "\n";
        }
    }

    virtual void OnResize(int width, int height) override
    {
        if (m_Width != width || m_Height != height)
        {
            m_Width = std::max(1, width);
            m_Height = std::max(1, height);

            m_CommandQueue.WaitForIdle();

            // 1. Reset Swap Chain Buffers
            for (int i = 0; i < m_NumFrames; ++i)
            {
                m_BackBuffers[i].Reset();
                m_FrameFenceValues[i] = m_CommandQueue.GetLastCompletedFenceValue();
            }

            // 2. Resize Swap Chain
            DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
            m_SwapChain->GetDesc(&swapChainDesc);
            m_SwapChain->ResizeBuffers(m_NumFrames, m_Width, m_Height,
                swapChainDesc.BufferDesc.Format, swapChainDesc.Flags);

            m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();
            UpdateRenderTargetViews(m_Device, m_SwapChain, m_RTVDescriptorHeap);

            // 3. Update Viewport 
            m_Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f,
                static_cast<float>(width), static_cast<float>(height));

            // 4. Resize Depth Buffer 
            ResizeDepthBuffer(width, height);
        }
    }

private:
    // ------------------- Helper Functions ------------------- //
    ComPtr<IDXGIAdapter4> GetAdapter(bool useWarp)
    {
        ComPtr<IDXGIFactory4> dxgiFactory;
        UINT createFactoryFlags = 0;
#if defined(_DEBUG)
        createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif
        ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

        ComPtr<IDXGIAdapter1> dxgiAdapter1;
        ComPtr<IDXGIAdapter4> dxgiAdapter4;

        if (useWarp)
        {
            ThrowIfFailed(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter1)));
            ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
        }
        else
        {
            SIZE_T maxDedicatedVideoMemory = 0;
            for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
            {
                DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
                dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

                if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
                    SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) &&
                    dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
                {
                    maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
                    ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
                }
            }
        }
        return dxgiAdapter4;
    }

    ComPtr<IDXGISwapChain4> CreateSwapChain(HWND hWnd, CommandQueue& commandQueue, uint32_t width, uint32_t height, uint32_t bufferCount)
    {
        ComPtr<IDXGISwapChain4> dxgiSwapChain4;
        ComPtr<IDXGIFactory4> dxgiFactory4;
        UINT createFactoryFlags = 0;
#if defined(_DEBUG)
        createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

        ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)));

        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.Width = width;
        swapChainDesc.Height = height;
        swapChainDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;    // 10-bit HDR color-space
        swapChainDesc.Stereo = FALSE;
        swapChainDesc.SampleDesc = { 1, 0 };
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = bufferCount;
        swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        swapChainDesc.Flags = CheckTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0; 

        ComPtr<IDXGISwapChain1> swapChain1;
        ThrowIfFailed(dxgiFactory4->CreateSwapChainForHwnd(
            commandQueue.GetCommandQueue(), hWnd, &swapChainDesc, nullptr, nullptr, &swapChain1));

        ThrowIfFailed(dxgiFactory4->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));
        ThrowIfFailed(swapChain1.As(&dxgiSwapChain4));
        return dxgiSwapChain4;
    }

    ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ComPtr<ID3D12Device2> device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors)
    {
        ComPtr<ID3D12DescriptorHeap> descriptorHeap;
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.NumDescriptors = numDescriptors;
        desc.Type = type;
        ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)));
        return descriptorHeap;
    }

    void UpdateRenderTargetViews(ComPtr<ID3D12Device2> device, ComPtr<IDXGISwapChain4> swapChain, ComPtr<ID3D12DescriptorHeap> descriptorHeap)
    {
        auto rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(descriptorHeap->GetCPUDescriptorHandleForHeapStart());

        for (int i = 0; i < m_NumFrames; ++i)
        {
            ComPtr<ID3D12Resource> backBuffer;
            ThrowIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));
            device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandle);
            m_BackBuffers[i] = backBuffer;
            rtvHandle.Offset(rtvDescriptorSize);
        }
    }

    bool CheckTearingSupport()
    {
        BOOL allowTearing = FALSE;

        // Rather than create the DXGI 1.5 factory interface directly, we create the
        // DXGI 1.4 interface and query for the 1.5 interface. This is to enable the 
        // graphics debugging tools which will not support the 1.5 factory interface 
        // until a future update.
        ComPtr<IDXGIFactory4> factory4;
        if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4))))
        {
            ComPtr<IDXGIFactory5> factory5;
            if (SUCCEEDED(factory4.As(&factory5)))
            {
                if (FAILED(factory5->CheckFeatureSupport(
                    DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                    &allowTearing, sizeof(allowTearing))))
                {
                    allowTearing = FALSE;
                }
            }
        }

        return allowTearing == TRUE;
    }

    // --- GraphicsContext Helpers (MiniEngine Style) ---
    // In MiniEngine, these live in the CommandContext class.
    // I implement them here for now, but will move them later.

    void TransitionResource(ComPtr<ID3D12GraphicsCommandList> cmdList,
        ComPtr<ID3D12Resource> resource,
        D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState)
    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            resource.Get(), beforeState, afterState);
        cmdList->ResourceBarrier(1, &barrier);
    }

    void ClearRTV(ComPtr<ID3D12GraphicsCommandList> cmdList,
        D3D12_CPU_DESCRIPTOR_HANDLE rtv, FLOAT* clearColor)
    {
        cmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    }

    void ClearDepth(ComPtr<ID3D12GraphicsCommandList> cmdList,
        D3D12_CPU_DESCRIPTOR_HANDLE dsv, FLOAT depth = 1.0f)
    {
        cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
    }

    // --- Helper to Upload Data to GPU ---
    // MiniEngine handles this in CommandContext::InitializeBuffer
    void UpdateBufferResource(ComPtr<ID3D12GraphicsCommandList> cmdList,
        ID3D12Resource** pDestinationResource, ID3D12Resource** pIntermediateResource,
        size_t numElements, size_t elementSize, const void* bufferData,
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE)
    {
        size_t bufferSize = numElements * elementSize;

        // Create the actual GPU buffer (Default Heap)
        // FIX: Create named variables so we can take their address
        auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags);

        // Create the actual GPU buffer (Default Heap)
        ThrowIfFailed(m_Device->CreateCommittedResource(
            &defaultHeapProperties, //&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc, //&CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags),
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(pDestinationResource)));

        // Create the upload buffer (Upload Heap)
        if (bufferData)
        {
            // FIX: Create named variables here too
            auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            auto uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

            ThrowIfFailed(m_Device->CreateCommittedResource(
                &uploadHeapProperties,  //&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                D3D12_HEAP_FLAG_NONE,
                &uploadBufferDesc,   //&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(pIntermediateResource)));

            D3D12_SUBRESOURCE_DATA subresourceData = {};
            subresourceData.pData = bufferData;
            subresourceData.RowPitch = bufferSize;
            subresourceData.SlicePitch = subresourceData.RowPitch;

            UpdateSubresources(cmdList.Get(), *pDestinationResource, *pIntermediateResource, 0, 0, 1, &subresourceData);
        }
    }

    void ResizeDepthBuffer(int width, int height)
    {
        if (m_IsInitialized)
        {
            // Flush any GPU commands that might be referencing the depth buffer.
            m_CommandQueue.WaitForIdle();

            width = std::max(1, width);
            height = std::max(1, height);

            // Resize screen dependent resources.
            // Create a depth buffer.
            D3D12_CLEAR_VALUE optimizedClearValue = {};
            optimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
            optimizedClearValue.DepthStencil = { 1.0f, 0 };

            auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            auto texDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height,
                1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

            ThrowIfFailed(m_Device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &texDesc,
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                &optimizedClearValue,
                IID_PPV_ARGS(&m_DepthBuffer)
            ));

            // Update the depth-stencil view.
            D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
            dsv.Format = DXGI_FORMAT_D32_FLOAT;
            dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            dsv.Texture2D.MipSlice = 0;
            dsv.Flags = D3D12_DSV_FLAG_NONE;

            m_Device->CreateDepthStencilView(m_DepthBuffer.Get(), &dsv,
                m_DSVHeap->GetCPUDescriptorHandleForHeapStart());
        }
    }

private:
    static const uint8_t m_NumFrames = 3;
    bool m_IsInitialized = false;
    bool m_TearingSupported{}; // Initialize in Constructor
    bool m_UseWarp;     // Windows Advanced Rasterization Platform (For software rasterization)

    CommandQueue m_CommandQueue;
    ComPtr<ID3D12Device2> m_Device;
    ComPtr<IDXGISwapChain4> m_SwapChain;
    ComPtr<ID3D12Resource> m_BackBuffers[m_NumFrames];
    ComPtr<ID3D12GraphicsCommandList> m_CommandList;
    ComPtr<ID3D12DescriptorHeap> m_RTVDescriptorHeap;
    UINT m_RTVDescriptorSize;
    UINT m_CurrentBackBufferIndex;
    uint64_t m_FrameFenceValues[m_NumFrames] = {};

    // =========== Data Members for Geometry Rendering  =================== //
    // --- Pipeline Objects ---
    ComPtr<ID3D12RootSignature> m_RootSignature;
    ComPtr<ID3D12PipelineState> m_PipelineState;

    // --- Data Buffers ---
    ComPtr<ID3D12Resource> m_VertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView;

    ComPtr<ID3D12Resource> m_IndexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_IndexBufferView;

    // --- Depth Buffer ---
    ComPtr<ID3D12Resource> m_DepthBuffer;
    ComPtr<ID3D12DescriptorHeap> m_DSVHeap;

    // --- Viewport & Scissor ---
    D3D12_VIEWPORT m_Viewport;
    D3D12_RECT m_ScissorRect;

    // --- Math / Matrices ---
    float m_FoV;
    DirectX::XMMATRIX m_ModelMatrix;
    DirectX::XMMATRIX m_ViewMatrix;
    DirectX::XMMATRIX m_ProjectionMatrix;

    bool m_ContentLoaded = false;
};

int main(int argc, char* argv[])
{
    HD2D_Renderer theApp;
    return GameCore::RunApplication(theApp, "");
}