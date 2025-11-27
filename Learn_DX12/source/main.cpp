
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

// --- Engine Includes ---
#include "../include/helpers.hpp"
#include "../include/CommandQueue.hpp"

#include "../include/GameCore.hpp"
#include "../include/GraphicsCore.hpp"
#include "../include/Display.hpp"
#include "../include/GpuResource.hpp"
#include "../include/GpuBuffer.hpp" 
#include "../include/ColorBuffer.hpp"
#include "../include/DepthBuffer.hpp"
#include "../include/DescriptorHeap.hpp"
#include "../include/RootSignature.hpp"
#include "../include/PipelineState.hpp"

#include "../include/CommandContext.hpp"
#include "../include/Shapes.hpp"

// --- Namespaces ---
using namespace Microsoft::WRL; // For ComPtr
using namespace DirectX;    //temporariliy put the directx namespace here for the dxmath

// ----------- The Application Class -----------
class HD2D_Renderer : public Game
{
public:
    HD2D_Renderer()
        : Game{L"Learn DX12 - HD 2D renderer", 1280, 720, true}
        , m_UseWarp{false}
        , m_ScissorRect{CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX)}
        , m_Viewport{CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(1280), static_cast<float>(720))}
        , m_FoV{45.0f} 
        , m_ContentLoaded{false}
    {
    }

    virtual void Startup() override
    {
        // 1. Initialize Graphics (Creates Device)
        Graphics::Initialize(m_UseWarp);

        Display::Initialize();

        m_IsInitialized = true;

        // ======== Load Content ============= //
        
        // A. Create a Command List for Uploading Data
        // Later on in the engine, might have to use a separate Copy Queue, but Direct Queue works fine for now.
        // reuse m_CommandList which was closed in step 6.

        // A. Initialize Shapes (Creates & Uploads Buffers)
        Graphics::Shapes::InitializeBox(m_VertexBuffer, m_IndexBuffer);

        // B. Upload Vertex Buffer

        // Create the resource first (Default Heap)
        /* small block to create the destination buffer first.In MiniEngine, GpuBuffer::Create handles this.
           Since we only have the base GpuResource right now, we create the committed resource manually(just 5 lines), 
           and then let InitializeBuffer handle the complex upload part.*/

           // B. Create Views
           // We know the box has 8 vertices and 36 indices.
           // Afterwards, the Model class would hold these views.

        // D. Create DSV Descriptor Heap (Already done in 5.)

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
        m_RootSignature.Reset(1, 0);
        m_RootSignature.InitAsConstants(0, sizeof(XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
        m_RootSignature.Finalize(L"Main Root Sig", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        // H. Create Pipeline State Object (PSO)
        m_PipelineState.SetRootSignature(m_RootSignature);
        m_PipelineState.SetInputLayout(std::vector<D3D12_INPUT_ELEMENT_DESC>(inputLayout, inputLayout + _countof(inputLayout)));
        m_PipelineState.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
        m_PipelineState.SetVertexShader(vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize());
        m_PipelineState.SetPixelShader(pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize());
        m_PipelineState.SetRenderTargetFormat(DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_D32_FLOAT);
        m_PipelineState.Finalize(L"Main PSO");

        // I. Create Depth Buffer & descriptorHeap for it
        m_DSVHeap.Create(Graphics::g_Device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
        m_DepthBuffer.Create(L"Scene Depth Buffer", Display::GetWidth(), Display::GetHeight(), DXGI_FORMAT_D32_FLOAT, m_DSVHeap);

        m_ContentLoaded = true;

        // Finish and Execute Command List (Shapes::InitializeBox handled it) 
    }

    virtual void Cleanup() override
    {
        Graphics::g_CommandQueue.WaitForIdle();
        Graphics::g_CommandQueue.Shutdown();

        Graphics::Shutdown();
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

        // 1. Create Context
        GraphicsContext Context(Graphics::g_CommandQueue);

        // 2. Get Current Back Buffer
        ColorBuffer& backBuffer = Display::GetCurrentBuffer();

        // 3. Transition & Clear
        Context.TransitionResource(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
        Context.TransitionResource(m_DepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);

        Context.ClearColor(backBuffer);
        Context.ClearDepth(m_DepthBuffer);

        // 4. Set State
        Context.SetRootSignature(m_RootSignature);
        Context.SetPipelineState(m_PipelineState);
        Context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        Context.SetViewport(m_Viewport);
        Context.SetScissor(m_ScissorRect);

        // 5. Bind Targets
        Context.SetRenderTarget(backBuffer.GetRTV(), m_DepthBuffer.GetDSV());

        // 6. Bind Buffers
        Context.SetVertexBuffer(0, m_VertexBuffer.VertexBufferView());
        Context.SetIndexBuffer(m_IndexBuffer.IndexBufferView());

        // 7. Update Constants
        XMMATRIX mvpMatrix = XMMatrixMultiply(m_ModelMatrix, m_ViewMatrix);
        mvpMatrix = XMMatrixMultiply(mvpMatrix, m_ProjectionMatrix);
        Context.SetConstants(0, sizeof(XMMATRIX) / 4, &mvpMatrix);

        // 8. Draw
        Context.DrawIndexed(Graphics::Shapes::BoxIndexCount);

        // 9. Transition to Present
        Context.TransitionResource(backBuffer, D3D12_RESOURCE_STATE_PRESENT);

        // 10. Finish (Close & Execute)
        Context.Finish();

        // 11. Present
        Display::Present(m_VSync, Display::CheckTearingSupport());
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
        // Check if size actually changed
        if (Display::GetWidth() != width || Display::GetHeight() != height)
        {
            // 1. Resize Display (SwapChain & RTVs)
            Display::Resize(width, height);

            // 2. Resize Depth Buffer 
            m_DepthBuffer.Destroy();
            m_DepthBuffer.Create(L"Scene Depth Buffer", width, height, DXGI_FORMAT_D32_FLOAT, m_DSVHeap);

            // 3. Update Viewport
            m_Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, (float)width, (float)height);
        }
    }

private:
    static const uint8_t m_NumFrames = 3;
    bool m_IsInitialized = false;
    bool m_UseWarp;     // Windows Advanced Rasterization Platform (For software rasterization)

    DescriptorHeap m_DSVHeap;

    // =========== Data Members for Geometry Rendering  =================== //
    // --- Pipeline Objects ---
    RootSignature m_RootSignature;
    GraphicsPipelineState m_PipelineState;

    // --- Data Buffers ---
    GpuBuffer m_VertexBuffer;
    GpuBuffer m_IndexBuffer;

    // --- Depth Buffer ---
    DepthBuffer m_DepthBuffer;

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