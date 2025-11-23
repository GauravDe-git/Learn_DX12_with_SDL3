// --- SDL3 (for windowing, input, and main loop) ---
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_main.h>
//#include <SDL3/SDL_syswm.h> for getting native hwnd? changed in SDL3?

// --- DirectX 12 specific headers ---
#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <wrl.h> // <-- For ComPtr (smart pointers)

// --- D3d12 Extension Library ---
#include <d3dx12.h>

// --- Windows / C++ Conflict Fix ---
// Fixes conflicts with Windows.h macros
// Probably not needed if only using SDL3? (As it adds those macros itself) 
#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

#include <iostream>
#include <cassert>
#include <chrono>
#include <algorithm>

// --- Helpers ---
#include "../include/helpers.hpp"

// --- Namespaces ---
using namespace Microsoft::WRL; // For ComPtr

// --- 3DGEP Global Configuration Variables ---
const uint8_t g_NumFrames = 3;      // Number of swap chain back buffers
bool g_UseWarp = false;             // Use software rasterizer?
uint32_t g_ClientWidth = 1280;      // Window width
uint32_t g_ClientHeight = 720;      // Window height
bool g_IsInitialized = false;       // Set to true once DX12 is ready

// --- DirectX 12 & Windowing Globals ---
HWND g_hWnd;                        // Handle to the OS Window
RECT g_WindowRect;                  // Window rectangle (for fullscreen toggle)
SDL_Window* g_Window;               // The Window from SDL

ComPtr<ID3D12Device2>               g_Device;
ComPtr<ID3D12CommandQueue>          g_CommandQueue;
ComPtr<IDXGISwapChain4>             g_SwapChain;
ComPtr<ID3D12Resource>              g_BackBuffers[g_NumFrames];
ComPtr<ID3D12GraphicsCommandList>   g_CommandList;
ComPtr<ID3D12CommandAllocator>      g_CommandAllocators[g_NumFrames];
ComPtr<ID3D12DescriptorHeap>        g_RTVDescriptorHeap;
UINT g_RTVDescriptorSize;
UINT g_CurrentBackBufferIndex;

// --- Synchronization objects ---
ComPtr<ID3D12Fence>                 g_Fence;
uint64_t                            g_FenceValue = 0;
uint64_t                            g_FrameFenceValues[g_NumFrames] = {};
HANDLE                              g_FenceEvent;

// --- Swap Chain & Window State ---
bool g_VSync                    = true;
bool g_TearingSupported         = false;
bool g_Fullscreen               = false;



// ---- Function Declarations ------
#pragma region fn declarations
void EnableDebugLayer();
ComPtr<IDXGIAdapter4> GetAdapter(bool useWarp);
ComPtr<ID3D12Device2> CreateDevice(ComPtr<IDXGIAdapter4> adapter);
ComPtr<ID3D12CommandQueue> CreateCommandQueue(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type);
bool CheckTearingSupport();
ComPtr<IDXGISwapChain4> CreateSwapChain(HWND hWnd, ComPtr<ID3D12CommandQueue> commandQueue, uint32_t width, uint32_t height, uint32_t bufferCount);
ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ComPtr<ID3D12Device2> device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors);
void UpdateRenderTargetViews(ComPtr<ID3D12Device2> device, ComPtr<IDXGISwapChain4> swapChain, ComPtr<ID3D12DescriptorHeap> descriptorHeap);
ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type);
ComPtr<ID3D12GraphicsCommandList> CreateCommandList(ComPtr<ID3D12Device2> device, ComPtr<ID3D12CommandAllocator> commandAllocator, D3D12_COMMAND_LIST_TYPE type);
ComPtr<ID3D12Fence> CreateFence(ComPtr<ID3D12Device2> device);
HANDLE CreateEventHandle();

// ------------- Helpers for Synchronization ------------- //
uint64_t Signal(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence, uint64_t& fenceValue);
void WaitForFenceValue(ComPtr<ID3D12Fence> fence, uint64_t fenceValue, HANDLE fenceEvent, std::chrono::milliseconds duration);
void Flush(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence, uint64_t& fenceValue, HANDLE fenceEvent);

// ------------- Update,Render,Resize Functions ------------------//
void Update();
void Render();
void Resize(uint32_t width, uint32_t height);
void SetFullscreen(bool fullscreen);
#pragma endregion

int main(int argc, char* argv[])
{
	if(!SDL_Init(SDL_INIT_VIDEO))
	{
		std::cerr << "Unable to initialize SDL: " << SDL_GetError() << '\n';
		SDL_Quit();
		return 1;
	}

	g_Window = SDL_CreateWindow("Learn DirectX 12", g_ClientWidth, g_ClientHeight, SDL_WINDOW_RESIZABLE);
	if (!g_Window)
	{
		std::cerr << "Could not create window:  "<< SDL_GetError() << '\n';
		SDL_Quit();
		return 1;
	}

    // --- Get the Native HWND for DirectX ---
    // This is the bridge between SDL and DX12
    g_hWnd = (HWND)SDL_GetPointerProperty(
        SDL_GetWindowProperties(g_Window),
        SDL_PROP_WINDOW_WIN32_HWND_POINTER,
        nullptr
    );
    if (!g_hWnd)
    {
        std::cerr << "Could not get HWND from window:  " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    // --- Initialize g_WindowRect ---
    // Now that we have the HWND, get its client area rectangle
    GetClientRect(g_hWnd, &g_WindowRect);
    

    // --- DirectX 12 Initialization Goes Here ---
    // Now have an 'hwnd' to create swap chain against.
     try
    {
        // --- DirectX 12 Initialization Goes Here ---
        // Call this FIRST, before any other DX12 code.
        EnableDebugLayer();

        // Create the DXGI Factory and get the adapter
        std::cout << "Looking for a compatible DirectX 12 adapter...\n";
        ComPtr<IDXGIAdapter4> adapter = GetAdapter(g_UseWarp);
        if (!adapter)
        {
            throw std::exception("No compatible DirectX 12 adapter found.");
        }
        std::cout << "Adapter found. Ready to create device.\n";

        // Create the D3D12 Device
        g_Device = CreateDevice(adapter);
        std::cout << "D3D12 device created successfully.\n";

        // Create the Command Queue
        g_CommandQueue = CreateCommandQueue(g_Device, D3D12_COMMAND_LIST_TYPE_DIRECT);
        std::cout << "Command queue created successfully.\n";

        // Check for Tearing Support
        g_TearingSupported = CheckTearingSupport();
        std::cout << "Tearing support: " << (g_TearingSupported ? "Enabled" : "Disabled") << "\n";

        // Create the Swap Chain
        g_SwapChain = CreateSwapChain(g_hWnd, g_CommandQueue, g_ClientWidth, g_ClientHeight, g_NumFrames);
        std::cout << "Swap chain created successfully.\n";

        // Create the RTV Descriptor Heap
        g_RTVDescriptorHeap = CreateDescriptorHeap(g_Device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, g_NumFrames);
        std::cout << "RTV descriptor heap created successfully.\n";
        g_RTVDescriptorSize = g_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        // Create the Render Target Views
        // This function will get the swap chain buffers and create RTVs for them
        // in the descriptor heap. It also stores the back buffer pointers
        // in our g_BackBuffers array.
        UpdateRenderTargetViews(g_Device, g_SwapChain, g_RTVDescriptorHeap);
        std::cout << "Render Target Views created successfully.\n";

        // Create the Command Allocators
        // We need one allocator for each frame in flight.
        for (int i = 0; i < g_NumFrames; ++i)
        {
            g_CommandAllocators[i] = CreateCommandAllocator(g_Device, D3D12_COMMAND_LIST_TYPE_DIRECT);
        }
        std::cout << g_NumFrames << "Command allocators created successfully.\n";

        // Create the Command List
        // Create the command list using the first allocator.
        // It will be reset in the game loop to use the correct allocator for the current frame.
        g_CommandList = CreateCommandList(g_Device, g_CommandAllocators[0], D3D12_COMMAND_LIST_TYPE_DIRECT);
        std::cout << "Command list created successfully.\n";

        // Create the Fence
        g_Fence = CreateFence(g_Device);
        std::cout << "Fence created successfully.\n";

        // Create the Fence Event
        g_FenceEvent = CreateEventHandle();
        std::cout << "Fence event created successfully.\n";

        // --- Finish Initialization ---
        // Get the index of the current back buffer.
        g_CurrentBackBufferIndex = g_SwapChain->GetCurrentBackBufferIndex();

        // Mark initialization as complete.
        g_IsInitialized = true;
        std::cout << "DirectX 12 Initialization Complete. Entering main loop...\n";

        // Example:
        // ThrowIfFailed(D3D12CreateDevice(...)); 
  
        // ID3D12Device* device = CreateDevice();
        // IDXGISwapChain* swapChain = CreateSwapChain(hwnd, ...);
        // ...etc
    }
    catch (std::exception& e)
    {
        std::cerr << "DX12 Init Failed: " << e.what() << '\n';
        return 1;
    }

	std::cout << "Successfully created window and got HWND. Ready for DX12 \n";

    // --- Main Game Loop ---
    bool running = true;
    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                }
                if (event.key.key == SDLK_F11)
                {
                    SetFullscreen(!g_Fullscreen);
                }
                if (event.key.key == SDLK_V) {
                    g_VSync = !g_VSync;
                    std::cout << "VSync: " << (g_VSync ? "On" : "Off") << "\n";
                }
            }
            if (event.type == SDL_EVENT_WINDOW_RESIZED)
            {
                // Only resize if DX12 is initialized and the size is not 0
                if (g_IsInitialized && event.window.data1 > 0 && event.window.data2 > 0)
                {
                    std::cout << "Window resizing to " << event.window.data1 << "x" << event.window.data2 << "\n";
                    Resize(event.window.data1, event.window.data2);
                }
            }
        }
        // --- Call Update once per frame ---
        Update();

        // --- DX12 Render Call Goes Here ---
        if (g_IsInitialized)
        {
            Render();
        }
    }

    // --- Cleanup ---

    // --- Wait for GPU to finish ---
    // Make sure the GPU is no longer using any resources before we clean up.
    if (g_IsInitialized)
    {
        Flush(g_CommandQueue, g_Fence, g_FenceValue, g_FenceEvent);
    }

    // Destroy DX12 objects...
    // ComPtrs will auto-release here.
    // Close the event handle.
    ::CloseHandle(g_FenceEvent);

    SDL_DestroyWindow(g_Window);
    SDL_Quit();

    return 0;
}

void EnableDebugLayer()
{
#if defined(_DEBUG)
    // Always enable the debug layer before doing anything DX12 related
    // so all possible errors generated while creating DX12 objects
    // are caught by the debug layer.
    ComPtr<ID3D12Debug> debugInterface;
    ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
    debugInterface->EnableDebugLayer();
#endif
}

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

            // Check to see if the adapter can create a D3D12 device without actually 
            // creating it. The adapter with the largest dedicated video memory
            // is favored.
            if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
                SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(),
                    D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) &&
                dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
            {
                maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
                ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
            }
        }
    }

    return dxgiAdapter4;
}

ComPtr<ID3D12Device2> CreateDevice(ComPtr<IDXGIAdapter4> adapter)
{
    ComPtr<ID3D12Device2> d3d12Device2;
    ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12Device2)));

    // Enable debug messages in debug mode.
#if defined(_DEBUG)
    ComPtr<ID3D12InfoQueue> pInfoQueue;
    if (SUCCEEDED(d3d12Device2.As(&pInfoQueue)))
    {
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

        // Suppress messages based on their severity level
        D3D12_MESSAGE_SEVERITY Severities[] =
        {
            D3D12_MESSAGE_SEVERITY_INFO
        };

        // Suppress individual messages by their ID
        D3D12_MESSAGE_ID DenyIds[] = {
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
        };

        D3D12_INFO_QUEUE_FILTER NewFilter = {};
        NewFilter.DenyList.NumSeverities = _countof(Severities);
        NewFilter.DenyList.pSeverityList = Severities;
        NewFilter.DenyList.NumIDs = _countof(DenyIds);
        NewFilter.DenyList.pIDList = DenyIds;

        ThrowIfFailed(pInfoQueue->PushStorageFilter(&NewFilter));
    }
#endif

    return d3d12Device2;
}

ComPtr<ID3D12CommandQueue> CreateCommandQueue(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type)
{
    ComPtr<ID3D12CommandQueue> d3d12CommandQueue;

    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = type;
    desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask = 0;

    ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&d3d12CommandQueue)));

    return d3d12CommandQueue;
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

ComPtr<IDXGISwapChain4> CreateSwapChain(HWND hWnd,
    ComPtr<ID3D12CommandQueue> commandQueue,
    uint32_t width, uint32_t height, uint32_t bufferCount)
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
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc = { 1, 0 };
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = bufferCount;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    // It is recommended to always allow tearing if tearing support is available.
    swapChainDesc.Flags = CheckTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    ComPtr<IDXGISwapChain1> swapChain1;
    ThrowIfFailed(dxgiFactory4->CreateSwapChainForHwnd(
        commandQueue.Get(),
        hWnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1));

    // Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
    // will be handled manually by SDL.
    ThrowIfFailed(dxgiFactory4->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain1.As(&dxgiSwapChain4));

    return dxgiSwapChain4;
}

ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ComPtr<ID3D12Device2> device,
    D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors)
{
    ComPtr<ID3D12DescriptorHeap> descriptorHeap;

    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors = numDescriptors;
    desc.Type = type;

    ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)));

    return descriptorHeap;
}

void UpdateRenderTargetViews(ComPtr<ID3D12Device2> device,
    ComPtr<IDXGISwapChain4> swapChain, ComPtr<ID3D12DescriptorHeap> descriptorHeap)
{
    auto rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(descriptorHeap->GetCPUDescriptorHandleForHeapStart());

    for (int i = 0; i < g_NumFrames; ++i)
    {
        ComPtr<ID3D12Resource> backBuffer;
        ThrowIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

        device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandle);

        g_BackBuffers[i] = backBuffer;

        rtvHandle.Offset(rtvDescriptorSize);
    }
}

ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(ComPtr<ID3D12Device2> device,
    D3D12_COMMAND_LIST_TYPE type)
{
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ThrowIfFailed(device->CreateCommandAllocator(type, IID_PPV_ARGS(&commandAllocator)));

    return commandAllocator;
}

ComPtr<ID3D12GraphicsCommandList> CreateCommandList(ComPtr<ID3D12Device2> device,
    ComPtr<ID3D12CommandAllocator> commandAllocator, D3D12_COMMAND_LIST_TYPE type)
{
    ComPtr<ID3D12GraphicsCommandList> commandList;
    ThrowIfFailed(device->CreateCommandList(0, type, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

    // Command lists are created in the recording state.
    // Close it so it's ready to be Reset() for the first time.
    ThrowIfFailed(commandList->Close());

    return commandList;
}

ComPtr<ID3D12Fence> CreateFence(ComPtr<ID3D12Device2> device)
{
    ComPtr<ID3D12Fence> fence;

    ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));

    return fence;
}

HANDLE CreateEventHandle()
{
    HANDLE fenceEvent;

    fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
    assert(fenceEvent && "Failed to create fence event.");

    return fenceEvent;
}

uint64_t Signal(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence,
    uint64_t& fenceValue)
{
    uint64_t fenceValueForSignal = ++fenceValue;
    ThrowIfFailed(commandQueue->Signal(fence.Get(), fenceValueForSignal));

    return fenceValueForSignal;
}

void WaitForFenceValue(ComPtr<ID3D12Fence> fence, uint64_t fenceValue, HANDLE fenceEvent,
    std::chrono::milliseconds duration = std::chrono::milliseconds::max())
{
    if (fence->GetCompletedValue() < fenceValue)
    {
        ThrowIfFailed(fence->SetEventOnCompletion(fenceValue, fenceEvent));
        ::WaitForSingleObject(fenceEvent, static_cast<DWORD>(duration.count()));
    }
}

void Flush(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence,
    uint64_t& fenceValue, HANDLE fenceEvent)
{
    uint64_t fenceValueForSignal = Signal(commandQueue, fence, fenceValue);
    WaitForFenceValue(fence, fenceValueForSignal, fenceEvent);
}



// --- Update and Render Functions ---- //
void Update()
{
    static uint64_t frameCounter = 0;
    static double elapsedSeconds = 0.0;
    static std::chrono::high_resolution_clock clock;
    static auto t0 = clock.now();

    frameCounter++;
    auto t1 = clock.now();
    auto deltaTime = t1 - t0;
    t0 = t1;

    elapsedSeconds += deltaTime.count() * 1e-9;
    if (elapsedSeconds > 1.0)
    {
        char buffer[500];
        auto fps = frameCounter / elapsedSeconds;
        sprintf_s(buffer, 500, "FPS: %f\n", fps);
        OutputDebugStringA(buffer); // This prints to the VS "Output" window

        frameCounter = 0;
        elapsedSeconds = 0.0;
    }
}

void Render()
{
    /*auto commandAllocator = g_CommandAllocators[g_CurrentBackBufferIndex];
    auto backBuffer = g_BackBuffers[g_CurrentBackBufferIndex];

    commandAllocator->Reset();
    g_CommandList->Reset(commandAllocator.Get(), nullptr);

    // Clear the render target.
    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            backBuffer.Get(),
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

        g_CommandList->ResourceBarrier(1, &barrier);

        FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(g_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
            g_CurrentBackBufferIndex, g_RTVDescriptorSize);

        g_CommandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    }

    // Present
    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            backBuffer.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        g_CommandList->ResourceBarrier(1, &barrier);

        ThrowIfFailed(g_CommandList->Close());

        ID3D12CommandList* const commandLists[] = {
            g_CommandList.Get()
        };
        g_CommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

        UINT syncInterval = g_VSync ? 1 : 0;
        UINT presentFlags = g_TearingSupported && !g_VSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
        ThrowIfFailed(g_SwapChain->Present(syncInterval, presentFlags));

        g_FrameFenceValues[g_CurrentBackBufferIndex] = Signal(g_CommandQueue, g_Fence, g_FenceValue);

        g_CurrentBackBufferIndex = g_SwapChain->GetCurrentBackBufferIndex();

        WaitForFenceValue(g_Fence, g_FrameFenceValues[g_CurrentBackBufferIndex], g_FenceEvent);
    }*/
    // 1. Get the allocator and buffer for the CURRENT frame.
    auto commandAllocator = g_CommandAllocators[g_CurrentBackBufferIndex];
    auto backBuffer = g_BackBuffers[g_CurrentBackBufferIndex];

    // 2. WAIT for the GPU to be finished with the resources for THIS frame.
    //    We check the fence value we stored *last time* we used this frame index.
    WaitForFenceValue(g_Fence, g_FrameFenceValues[g_CurrentBackBufferIndex], g_FenceEvent);

    // 3. NOW it is safe to reset the allocator and command list.
    commandAllocator->Reset();
    g_CommandList->Reset(commandAllocator.Get(), nullptr);

    // 4. Record commands (clear, transitions)
    // Clear the render target.
    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            backBuffer.Get(),
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

        g_CommandList->ResourceBarrier(1, &barrier);

        FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(g_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
            g_CurrentBackBufferIndex, g_RTVDescriptorSize);

        g_CommandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    }

    // Present
    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            backBuffer.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        g_CommandList->ResourceBarrier(1, &barrier);

        // 5. Close the command list. This should no longer throw an exception.
        ThrowIfFailed(g_CommandList->Close());

        // 6. Execute the commands.
        ID3D12CommandList* const commandLists[] = {
            g_CommandList.Get()
        };
        g_CommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

        // 7. Present the frame.
        UINT syncInterval = g_VSync ? 1 : 0;
        UINT presentFlags = g_TearingSupported && !g_VSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
        ThrowIfFailed(g_SwapChain->Present(syncInterval, presentFlags));

        // 8. Signal the fence and store the NEW value for THIS frame index.
        g_FrameFenceValues[g_CurrentBackBufferIndex] = Signal(g_CommandQueue, g_Fence, g_FenceValue);

        // 9. Get the index for the NEXT frame.
        g_CurrentBackBufferIndex = g_SwapChain->GetCurrentBackBufferIndex();
    }
}

void Resize(uint32_t width, uint32_t height)
{
    if (g_ClientWidth != width || g_ClientHeight != height)
    {
        // Don't allow 0 size swap chain back buffers.
        g_ClientWidth = std::max(1u, width);
        g_ClientHeight = std::max(1u, height);

        // Flush the GPU queue to make sure the swap chain's back buffers
        // are not being referenced by an in-flight command list.
        Flush(g_CommandQueue, g_Fence, g_FenceValue, g_FenceEvent);

        for (int i = 0; i < g_NumFrames; ++i)
        {
            // Any references to the back buffers must be released
            // before the swap chain can be resized.
            g_BackBuffers[i].Reset();
            //g_FrameFenceValues[i] = g_FrameFenceValues[g_CurrentBackBufferIndex];
            // Correct way: Set all fence values to the value we just flushed.
            g_FrameFenceValues[i] = g_FenceValue;
        }

        DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
        ThrowIfFailed(g_SwapChain->GetDesc(&swapChainDesc));
        ThrowIfFailed(g_SwapChain->ResizeBuffers(g_NumFrames, g_ClientWidth, g_ClientHeight,
            swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

        g_CurrentBackBufferIndex = g_SwapChain->GetCurrentBackBufferIndex();

        UpdateRenderTargetViews(g_Device, g_SwapChain, g_RTVDescriptorHeap);
    }
}

void SetFullscreen(bool fullscreen)
{
    if (g_Fullscreen != fullscreen)
    {
        g_Fullscreen = fullscreen;
        if (g_Fullscreen)
        {
            // This is the SDL3 equivalent of the *entire* Win32
            // style-changing, monitor-finding, and resizing block.
            // It defaults to "desktop fullscreen" (borderless).
            SDL_SetWindowFullscreen(g_Window, true);
        }
        else
        {
            // SDL3 automatically restores the window
            // to its previous windowed state and position.
            SDL_SetWindowFullscreen(g_Window, false);
        }
    }
}