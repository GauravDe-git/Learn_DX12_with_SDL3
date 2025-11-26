#include "../include/DescriptorHeap.hpp"

DescriptorHeap::DescriptorHeap()
{
    m_DescriptorSize = 0;
    m_NumFreeDescriptors = 0;
    m_CurrentHandle = { D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN };
}

void DescriptorHeap::Create(ID3D12Device* pDevice,
    D3D12_DESCRIPTOR_HEAP_TYPE Type,
    uint32_t MaxCount,
    bool bShaderVisible)
{
    m_HeapDesc.Type = Type;
    m_HeapDesc.NumDescriptors = MaxCount;
    m_HeapDesc.Flags = bShaderVisible
        ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
        : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    m_HeapDesc.NodeMask = 1;

    pDevice->CreateDescriptorHeap(&m_HeapDesc, IID_PPV_ARGS(&m_Heap));
    m_Heap->SetName(L"Descriptor Heap");

    m_DescriptorSize = pDevice->GetDescriptorHandleIncrementSize(Type);
    m_NumFreeDescriptors = MaxCount;
    m_CurrentHandle = m_Heap->GetCPUDescriptorHandleForHeapStart();
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::Alloc(uint32_t Count)
{
    if (Count > m_NumFreeDescriptors)
    {
        return { D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN };
    }

    D3D12_CPU_DESCRIPTOR_HANDLE ret = m_CurrentHandle;
    m_CurrentHandle.ptr += Count * m_DescriptorSize;
    m_NumFreeDescriptors -= Count;

    return ret;
}
