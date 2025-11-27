#include "../include/GraphicsCore.hpp"
#include "../include/helpers.hpp"
#include "../include/LinearAllocator.hpp"

#include <dxgi1_6.h>

#include <iostream>
using namespace Microsoft::WRL;

// We don't need to "initialize" linear the allocator explicitly because the static page manager initializes itself on first use (C++ magic). But we must clean it up.

namespace Graphics
{
    ComPtr<ID3D12Device2> g_Device = nullptr;

    CommandQueue g_CommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);

    // Helper to get the adapter (moved from main.cpp)
    ComPtr<IDXGIAdapter4> GetAdapter(bool useWarp)
    {
        ComPtr<IDXGIFactory4> dxgiFactory;
        UINT createFactoryFlags = 0;
#if defined(_DEBUG)
        createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif
        CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory));

        ComPtr<IDXGIAdapter1> dxgiAdapter1;
        ComPtr<IDXGIAdapter4> dxgiAdapter4;

        if (useWarp)
        {
            dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter1));
            dxgiAdapter1.As(&dxgiAdapter4);
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

    void Initialize(bool useWarp)
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
        ComPtr<IDXGIAdapter4> adapter = GetAdapter(useWarp);
        ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_Device)));

        // Initialize the Command Queue 
        g_CommandQueue.Create(g_Device.Get());
    }

    void Shutdown()
    {
        // Shut command_queue down before destroying the device
        g_CommandQueue.Shutdown();

        // Destroy all upload pages
        LinearAllocator::DestroyAll();

        g_Device.Reset();
    }
}