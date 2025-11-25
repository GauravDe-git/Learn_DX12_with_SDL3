
// --- DirectX 12 specific headers ---
#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <wrl.h> // <-- For ComPtr (smart pointers)

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


// ----------- The Application Class -----------
class HD2D_Renderer : public Game
{
public:
    HD2D_Renderer()
        : Game(L"Learn DX12 - HD 2D renderer", 1280, 720, true)
        , m_CommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT)
        , m_UseWrap{false}
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
        ComPtr<IDXGIAdapter4> adapter = GetAdapter(m_UseWrap); // can fall back to software rasterizer?
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
    }

    virtual void Cleanup() override
    {
        m_CommandQueue.WaitForIdle();
        m_CommandQueue.Shutdown();
    }

    virtual void Update(float deltaTime) override
    {
        // Update logic (FPS counter, etc.) can go here
        // For now, we just print FPS occasionally
        static double elapsedSeconds = 0.0;
        static uint64_t frameCounter = 0;

        elapsedSeconds += deltaTime;
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
    }

    virtual void Render() override
    {
        if (!m_IsInitialized) return;

        // 1. Request an Allocator 
        ID3D12CommandAllocator* allocator = m_CommandQueue.RequestAllocator();

        // 2. Wait for Previous Frame
        m_CommandQueue.WaitForFence(m_FrameFenceValues[m_CurrentBackBufferIndex]);
        
        // 3. Reset command List
        m_CommandList->Reset(allocator, nullptr);

        auto backBuffer = m_BackBuffers[m_CurrentBackBufferIndex];

        // -- RECORD COMMANDS -- //

        // Transition to Render Target
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            backBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_CommandList->ResourceBarrier(1, &barrier);

        // Clear
        FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(m_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
            m_CurrentBackBufferIndex, m_RTVDescriptorSize);
        m_CommandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

        // Transition to Present
        barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            backBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        m_CommandList->ResourceBarrier(1, &barrier);

        // Execute
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

            for (int i = 0; i < m_NumFrames; ++i)
            {
                m_BackBuffers[i].Reset();
                m_FrameFenceValues[i] = m_CommandQueue.GetLastCompletedFenceValue();
            }

            DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
            m_SwapChain->GetDesc(&swapChainDesc);
            m_SwapChain->ResizeBuffers(m_NumFrames, m_Width, m_Height,
                swapChainDesc.BufferDesc.Format, swapChainDesc.Flags);

            m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();
            UpdateRenderTargetViews(m_Device, m_SwapChain, m_RTVDescriptorHeap);
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

private:
    static const uint8_t m_NumFrames = 3;
    bool m_IsInitialized = false;
    bool m_TearingSupported = CheckTearingSupport();
    bool m_UseWrap;     //For software rasterization?

    CommandQueue m_CommandQueue;
    ComPtr<ID3D12Device2> m_Device;
    ComPtr<IDXGISwapChain4> m_SwapChain;
    ComPtr<ID3D12Resource> m_BackBuffers[m_NumFrames];
    ComPtr<ID3D12GraphicsCommandList> m_CommandList;
    ComPtr<ID3D12DescriptorHeap> m_RTVDescriptorHeap;
    UINT m_RTVDescriptorSize;
    UINT m_CurrentBackBufferIndex;
    uint64_t m_FrameFenceValues[m_NumFrames] = {};
};

int main(int argc, char* argv[])
{
    HD2D_Renderer theApp;
    return GameCore::RunApplication(theApp, "");
}