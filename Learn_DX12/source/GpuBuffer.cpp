#include "../include/GpuBuffer.hpp"
#include "../include/GraphicsCore.hpp"
#include "../include/CommandContext.hpp"
#include "../include/helpers.hpp"

#include <d3dx12.h>

void GpuBuffer::Create(const std::wstring& name, uint32_t NumElements, uint32_t ElementSize, const void* initialData)
{
    Destroy(); // Cleanup old resource if any

    m_ElementCount = NumElements;
    m_ElementSize = ElementSize;
    m_BufferSize = NumElements * ElementSize;

    // Create the committed resource (Default Heap)
    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto desc = CD3DX12_RESOURCE_DESC::Buffer(m_BufferSize, m_ResourceFlags);

    ThrowIfFailed(Graphics::g_Device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_COMMON, nullptr,
        IID_PPV_ARGS(m_pResource.GetAddressOf())));

    m_pResource->SetName(name.c_str());
    m_UsageState = D3D12_RESOURCE_STATE_COMMON;
    m_GpuVirtualAddress = m_pResource->GetGPUVirtualAddress();

    // If we have initial data, upload it!
    if (initialData)
    {
        GraphicsContext Context(Graphics::g_CommandQueue);
        Context.InitializeBuffer(*this, initialData, m_BufferSize);
        Context.Finish(true);
    }
}

D3D12_VERTEX_BUFFER_VIEW GpuBuffer::VertexBufferView(size_t Offset, uint32_t Size, uint32_t Stride) const
{
    D3D12_VERTEX_BUFFER_VIEW VBView;
    VBView.BufferLocation = m_GpuVirtualAddress + Offset;
    VBView.SizeInBytes = (Size == 0) ? (uint32_t)(m_BufferSize - Offset) : Size;
    VBView.StrideInBytes = (Stride == 0) ? m_ElementSize : Stride;
    return VBView;
}

D3D12_INDEX_BUFFER_VIEW GpuBuffer::IndexBufferView(size_t Offset, uint32_t Size, bool b32Bit) const
{
    D3D12_INDEX_BUFFER_VIEW IBView;
    IBView.BufferLocation = m_GpuVirtualAddress + Offset;
    IBView.Format = b32Bit ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
    IBView.SizeInBytes = (Size == 0) ? (uint32_t)(m_BufferSize - Offset) : Size;
    return IBView;
}