#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <cstdint>

#define D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN ((D3D12_GPU_VIRTUAL_ADDRESS)-1)

class DescriptorHeap
{
public:
    DescriptorHeap();

    void Create(ID3D12Device* pDevice,
        D3D12_DESCRIPTOR_HEAP_TYPE Type,
        uint32_t MaxCount,
        bool bShaderVisible = false);

    D3D12_CPU_DESCRIPTOR_HANDLE Alloc(uint32_t Count = 1);

    ID3D12DescriptorHeap* GetHeapPointer() const { return m_Heap.Get(); }
    uint32_t GetDescriptorSize() const { return m_DescriptorSize; }

private:
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_Heap;
    D3D12_DESCRIPTOR_HEAP_DESC m_HeapDesc = {};
    uint32_t m_DescriptorSize = 0;
    uint32_t m_NumFreeDescriptors = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE m_CurrentHandle = { D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN };
};
