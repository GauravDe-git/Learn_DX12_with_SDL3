#pragma once
#include <d3d12.h>
#include <wrl/client.h>

#include "d3dx12.h"

#include "GraphicsCore.hpp"
#include "CommandQueue.hpp"
#include "PipelineState.hpp"
#include "RootSignature.hpp"
#include "GpuResource.hpp"
#include "ColorBuffer.hpp"
#include "DepthBuffer.hpp"

class CommandContext
{
public:
    // Initialize with the queue it will submit to
    CommandContext(CommandQueue& Queue);
    virtual ~CommandContext();

    // Begin a new recording session (requests allocator, resets list)
    void Reset();

    // Close list, execute on queue, and return fence value
    uint64_t Finish(bool WaitForCompletion = false);

    // --- State Management ---
    void TransitionResource(GpuResource& Resource, D3D12_RESOURCE_STATES NewState, bool FlushImmediate = false);

    // --- Common Commands ---
    void CopyBuffer(GpuResource& Dest, GpuResource& Src);
    void CopyBufferRegion(GpuResource& Dest, size_t DestOffset, GpuResource& Src, size_t SrcOffset, size_t NumBytes);

    ID3D12GraphicsCommandList* GetCommandList() const { return m_CommandList.Get(); }

protected:
    CommandQueue& m_Queue;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_CommandList;
    ID3D12CommandAllocator* m_CurrentAllocator; // Borrowed from Queue
};

class GraphicsContext : public CommandContext
{
public:
    GraphicsContext(CommandQueue& Queue) : CommandContext(Queue) {}

    void ClearColor(ColorBuffer& Target, float* ClearColor = nullptr);
    void ClearDepth(DepthBuffer& Target);

    void SetRootSignature(const RootSignature& RootSig);
    void SetPipelineState(const GraphicsPipelineState& PSO);

    void SetRenderTargets(UINT NumRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE RTVs[], D3D12_CPU_DESCRIPTOR_HANDLE DSV);
    void SetRenderTarget(const D3D12_CPU_DESCRIPTOR_HANDLE RTV, D3D12_CPU_DESCRIPTOR_HANDLE DSV)
    {
        SetRenderTargets(1, &RTV, DSV);
    }

    void SetViewport(const D3D12_VIEWPORT& vp);
    void SetScissor(const D3D12_RECT& rect);

    void SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY Topology);

    void SetConstants(UINT RootIndex, UINT NumConstants, const void* pConstants);
    void SetConstantBuffer(UINT RootIndex, D3D12_GPU_VIRTUAL_ADDRESS CBV);

    void SetVertexBuffer(UINT Slot, const D3D12_VERTEX_BUFFER_VIEW& VBView);
    void SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& IBView);

    void Draw(UINT VertexCount, UINT VertexStartOffset = 0);
    void DrawIndexed(UINT IndexCount, UINT StartIndexLocation = 0, INT BaseVertexLocation = 0);
};