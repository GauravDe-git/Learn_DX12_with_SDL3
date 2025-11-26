#pragma once

#include <d3d12.h>
#include <wrl.h>

namespace Graphics
{
    // Global D3D12 Device
    extern Microsoft::WRL::ComPtr<ID3D12Device2> g_Device;

    // Initialize the graphics system (creates the device)
    void Initialize(bool useWarp = false);

    // Shutdown the graphics system
    void Shutdown();
}