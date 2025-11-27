#pragma once

#include <d3d12.h>
#include <wrl.h>

#define D3D12_GPU_VIRTUAL_ADDRESS_NULL      ((D3D12_GPU_VIRTUAL_ADDRESS)0)

class GpuResource
{
    //TODO: Implement Friend class simillar MiniEngine 
    friend class CommandContext;
    friend class GraphicsContext;
    friend class ComputeContext;

public:
    GpuResource() :
        m_GpuVirtualAddress(D3D12_GPU_VIRTUAL_ADDRESS_NULL),
        m_UsageState(D3D12_RESOURCE_STATE_COMMON),
        m_TransitioningState((D3D12_RESOURCE_STATES)-1)
    {
    }

    GpuResource(ID3D12Resource* pResource, D3D12_RESOURCE_STATES CurrentState) :
        m_GpuVirtualAddress(D3D12_GPU_VIRTUAL_ADDRESS_NULL),
        m_pResource(pResource),
        m_UsageState(CurrentState),
        m_TransitioningState((D3D12_RESOURCE_STATES)-1)
    {
    }

    virtual ~GpuResource() { Destroy(); }

    virtual void Destroy()
    {
        m_pResource = nullptr;
        m_GpuVirtualAddress = D3D12_GPU_VIRTUAL_ADDRESS_NULL;
    }

    // Accessors
    ID3D12Resource* operator->() { return m_pResource.Get(); }
    const ID3D12Resource* operator->() const { return m_pResource.Get(); }

    ID3D12Resource* GetResource() { return m_pResource.Get(); }
    const ID3D12Resource* GetResource() const { return m_pResource.Get(); }

    // Allows to pass &m_VertexBuffer to functions expecting ID3D12Resource**
    ID3D12Resource** GetAddressOf() { return m_pResource.GetAddressOf(); }

    D3D12_GPU_VIRTUAL_ADDRESS GetGpuVirtualAddress() const { return m_GpuVirtualAddress; }

    // Helper for state tracking
    D3D12_RESOURCE_STATES GetUsageState() const { return m_UsageState; }
    void SetUsageState(D3D12_RESOURCE_STATES State) { m_UsageState = State; }

    // Helper for initialization (Not in MiniEngine, but useful for refactoring)
    void SetResource(ID3D12Resource* pResource, D3D12_RESOURCE_STATES State)
    {
        m_pResource = pResource;
        m_UsageState = State;
    }
    D3D12_GPU_VIRTUAL_ADDRESS m_GpuVirtualAddress; //put in public for the linear allocator

protected:
    Microsoft::WRL::ComPtr<ID3D12Resource> m_pResource;
    D3D12_RESOURCE_STATES m_UsageState;
    D3D12_RESOURCE_STATES m_TransitioningState;
    
};
