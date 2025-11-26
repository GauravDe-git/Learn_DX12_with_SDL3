#include "../include/Display.hpp"
#include "../include/GraphicsCore.hpp"
#include "../include/GameCore.hpp" // For g_Window
#include "../include/CommandQueue.hpp"
#include "../include/DescriptorHeap.hpp"
#include "../include/helpers.hpp"

#include <dxgi1_6.h>

#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif
#include <algorithm> // For std::max

using namespace Microsoft::WRL;

// Import the global command queue from GraphicsCore 
namespace Graphics
{
    extern CommandQueue g_CommandQueue;
}

namespace Display
{
    // Internal State
    ComPtr<IDXGISwapChain4> m_SwapChain;
    const uint32_t SWAP_CHAIN_BUFFER_COUNT = 3;

    ColorBuffer m_DisplayPlane[SWAP_CHAIN_BUFFER_COUNT];
    DescriptorHeap m_RTVHeap;

    uint32_t m_Width = 1280;
    uint32_t m_Height = 720;
    uint32_t m_CurrentBackBufferIndex = 0;

    // Fence values for each frame to ensure we don't draw to a buffer the GPU is using
    uint64_t m_FrameFenceValues[SWAP_CHAIN_BUFFER_COUNT] = {};

    // Forward declaration of helper
    bool CheckTearingSupport();
}

void Display::Initialize(void)
{
    // 1. Get Window Handle
    HWND hwnd = (HWND)SDL_GetPointerProperty(
        SDL_GetWindowProperties(GameCore::g_Window),
        SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);

    // 2. Create Swap Chain
    ComPtr<IDXGIFactory4> dxgiFactory4;
    UINT createFactoryFlags = 0;
#if defined(_DEBUG)
    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif
    ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)));

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = m_Width;
    swapChainDesc.Height = m_Height;
    swapChainDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc = { 1, 0 };
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = SWAP_CHAIN_BUFFER_COUNT;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.Flags = CheckTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    ComPtr<IDXGISwapChain1> swapChain1;
    ThrowIfFailed(dxgiFactory4->CreateSwapChainForHwnd(
        Graphics::g_CommandQueue.GetCommandQueue(),
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1));

    ThrowIfFailed(dxgiFactory4->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
    ThrowIfFailed(swapChain1.As(&m_SwapChain));

    // 3. Create RTV Heap
    m_RTVHeap.Create(Graphics::g_Device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, SWAP_CHAIN_BUFFER_COUNT);

    // 4. Create Color Buffers
    for (uint32_t i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
    {
        ComPtr<ID3D12Resource> backBuffer;
        ThrowIfFailed(m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

        wchar_t name[32];
        swprintf(name, 32, L"Primary SwapChain Buffer %u", i);

        m_DisplayPlane[i].CreateFromSwapChain(name, backBuffer.Detach(), m_RTVHeap);

        // SET CLEAR COLOR HERE
        m_DisplayPlane[i].SetClearColor(Color(0.4f, 0.6f, 0.9f, 1.0f));
    }

    m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();
}

void Display::Shutdown(void)
{
    // 1. Wait for GPU to finish all work before destroying anything
    Graphics::g_CommandQueue.WaitForIdle();

    // 2. Destroy the ColorBuffers (wrappers)
    for (int i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
    {
        m_DisplayPlane[i].Destroy();
    }

    // 3. Release the Swap Chain
    m_SwapChain.Reset();

    // 4. Release the RTV Heap
    // DescriptorHeap class doesn't have a Destroy() method, 
    // but since it uses ComPtr internally, can just let it go out of scope,
    // OR you can add a Destroy() method to DescriptorHeap.hpp that calls m_Heap.Reset().
    // For now, doing nothing is ok as the destructor will handle it, 
    // but explicit cleanup is better.
}

void Display::Resize(uint32_t width, uint32_t height)
{
    if (m_Width == width && m_Height == height)
        return;

    m_Width = std::max(1u, width);
    m_Height = std::max(1u, height);

    Graphics::g_CommandQueue.WaitForIdle();

    for (int i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
    {
        m_DisplayPlane[i].Destroy();
        m_FrameFenceValues[i] = Graphics::g_CommandQueue.GetLastCompletedFenceValue();
    }

    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    m_SwapChain->GetDesc(&swapChainDesc);
    m_SwapChain->ResizeBuffers(SWAP_CHAIN_BUFFER_COUNT, m_Width, m_Height,
        swapChainDesc.BufferDesc.Format, swapChainDesc.Flags);

    m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();

    for (uint32_t i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
    {
        ComPtr<ID3D12Resource> backBuffer;
        ThrowIfFailed(m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

        wchar_t name[32];
        swprintf(name, 32, L"Primary SwapChain Buffer %u", i);

        m_DisplayPlane[i].CreateFromSwapChain(name, backBuffer.Detach(), m_RTVHeap);

        // SET CLEAR COLOR HERE TOO (For Resize)
        m_DisplayPlane[i].SetClearColor(Color(0.4f, 0.6f, 0.9f, 1.0f));
    }
}

void Display::Present(bool enableVSync, bool allowTearing)
{
    UINT syncInterval = enableVSync ? 1 : 0;
    UINT presentFlags = (allowTearing && !enableVSync) ? DXGI_PRESENT_ALLOW_TEARING : 0;

    m_SwapChain->Present(syncInterval, presentFlags);


    // Update fence for the buffer we just presented
    m_FrameFenceValues[m_CurrentBackBufferIndex] = Graphics::g_CommandQueue.GetNextFenceValue() - 1;

    // Move to next buffer
    m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();

    // Wait if the next buffer is still in use by GPU
    Graphics::g_CommandQueue.WaitForFence(m_FrameFenceValues[m_CurrentBackBufferIndex]);
}

ColorBuffer& Display::GetCurrentBuffer(void)
{
    return m_DisplayPlane[m_CurrentBackBufferIndex];
}

uint32_t Display::GetWidth(void)
{
    return m_Width;
}

uint32_t Display::GetHeight(void)
{
    return m_Height;
}

// Helper 
   bool Display::CheckTearingSupport()
    {
        BOOL allowTearing = FALSE;
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