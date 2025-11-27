#pragma once

#include "GpuResource.hpp"

#include <string>

class GpuBuffer : public GpuResource
{
public:
    virtual ~GpuBuffer() { Destroy(); }

    // Create a buffer. If initial data is provided, it will be copied into the buffer using the default command context.
    void Create(const std::wstring& name, uint32_t NumElements, uint32_t ElementSize, const void* initialData = nullptr);

    // Create views
    D3D12_VERTEX_BUFFER_VIEW VertexBufferView(size_t Offset = 0, uint32_t Size = 0, uint32_t Stride = 0) const;
    D3D12_INDEX_BUFFER_VIEW IndexBufferView(size_t Offset = 0, uint32_t Size = 0, bool b32Bit = false) const;

    // Getters
    size_t GetBufferSize() const { return m_BufferSize; }
    uint32_t GetElementCount() const { return m_ElementCount; }
    uint32_t GetElementSize() const { return m_ElementSize; }

protected:
    size_t m_BufferSize = 0;
    uint32_t m_ElementCount = 0;
    uint32_t m_ElementSize = 0;
    D3D12_RESOURCE_FLAGS m_ResourceFlags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
};