#include "../include/CommandContext.hpp"

using namespace Microsoft::WRL;

CommandContext::CommandContext(CommandQueue& Queue)
    : m_Queue(Queue), m_CurrentAllocator(nullptr)
{
    // Create the command list once. We will reset it later.
    // We need an initial allocator just to create it, but we can discard it immediately.
    ID3D12CommandAllocator* allocator = m_Queue.RequestAllocator();
    Graphics::g_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, nullptr, IID_PPV_ARGS(&m_CommandList));
    m_CommandList->Close();
    m_Queue.DiscardAllocator(0, allocator);
}

CommandContext::~CommandContext()
{
    if (m_CurrentAllocator)
    {
        // If we are destroying the context while holding an allocator, we must discard it.
        // Assuming 0 fence value since we didn't execute.
        m_Queue.DiscardAllocator(0, m_CurrentAllocator);
    }
}

void CommandContext::Reset()
{
    // 1. Get a fresh allocator from the queue
    m_CurrentAllocator = m_Queue.RequestAllocator();

    // 2. Reset the command list using this allocator
    m_CommandList->Reset(m_CurrentAllocator, nullptr);
}

uint64_t CommandContext::Finish(bool WaitForCompletion)
{
    // 1. Close the list
    m_CommandList->Close();

    // 2. Execute
    uint64_t fenceValue = m_Queue.ExecuteCommandList(m_CommandList.Get());

    // 3. Return the allocator to the pool (marked with the fence value)
    m_Queue.DiscardAllocator(fenceValue, m_CurrentAllocator);
    m_CurrentAllocator = nullptr;

    // 4. Optional wait
    if (WaitForCompletion)
    {
        m_Queue.WaitForFence(fenceValue);
    }

    return fenceValue;
}

void CommandContext::TransitionResource(GpuResource& Resource, D3D12_RESOURCE_STATES NewState, bool FlushImmediate)
{
    D3D12_RESOURCE_STATES oldState = Resource.GetUsageState();

    if (oldState != NewState)
    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            Resource.GetResource(), oldState, NewState);
        m_CommandList->ResourceBarrier(1, &barrier);
        Resource.SetUsageState(NewState);
    }
    // FlushImmediate logic could go here if we were batching barriers
}

void CommandContext::CopyBuffer(GpuResource& Dest, GpuResource& Src)
{
    TransitionResource(Dest, D3D12_RESOURCE_STATE_COPY_DEST);
    TransitionResource(Src, D3D12_RESOURCE_STATE_COPY_SOURCE);
    m_CommandList->CopyResource(Dest.GetResource(), Src.GetResource());
}

void CommandContext::CopyBufferRegion(GpuResource& Dest, size_t DestOffset, GpuResource& Src, size_t SrcOffset, size_t NumBytes)
{
    TransitionResource(Dest, D3D12_RESOURCE_STATE_COPY_DEST);
    TransitionResource(Src, D3D12_RESOURCE_STATE_COPY_SOURCE);
    m_CommandList->CopyBufferRegion(Dest.GetResource(), DestOffset, Src.GetResource(), SrcOffset, NumBytes);
}

// ========================================= //
// --- Graphics Context Implementation ---
// ========================================= //

void GraphicsContext::ClearColor(ColorBuffer& Target, float* ClearColor)
{
    m_CommandList->ClearRenderTargetView(Target.GetRTV(), ClearColor ? ClearColor : Target.GetClearColor().GetPtr(), 0, nullptr);
}

void GraphicsContext::ClearDepth(DepthBuffer& Target)
{
    m_CommandList->ClearDepthStencilView(Target.GetDSV(), D3D12_CLEAR_FLAG_DEPTH, Target.GetClearDepth(), Target.GetClearStencil(), 0, nullptr);
}

void GraphicsContext::SetRootSignature(const RootSignature& RootSig)
{
    m_CommandList->SetGraphicsRootSignature(RootSig.GetSignature());
}

void GraphicsContext::SetPipelineState(const GraphicsPipelineState& PSO)
{
    m_CommandList->SetPipelineState(PSO.GetPipelineStateObject());
}

void GraphicsContext::SetRenderTargets(UINT NumRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE RTVs[], D3D12_CPU_DESCRIPTOR_HANDLE DSV)
{
    m_CommandList->OMSetRenderTargets(NumRTVs, RTVs, FALSE, &DSV);
}

void GraphicsContext::SetViewport(const D3D12_VIEWPORT& vp)
{
    m_CommandList->RSSetViewports(1, &vp);
}

void GraphicsContext::SetScissor(const D3D12_RECT& rect)
{
    m_CommandList->RSSetScissorRects(1, &rect);
}

void GraphicsContext::SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY Topology)
{
    m_CommandList->IASetPrimitiveTopology(Topology);
}

void GraphicsContext::SetConstants(UINT RootIndex, UINT NumConstants, const void* pConstants)
{
    m_CommandList->SetGraphicsRoot32BitConstants(RootIndex, NumConstants, pConstants, 0);
}

void GraphicsContext::SetConstantBuffer(UINT RootIndex, D3D12_GPU_VIRTUAL_ADDRESS CBV)
{
    m_CommandList->SetGraphicsRootConstantBufferView(RootIndex, CBV);
}

void GraphicsContext::SetVertexBuffer(UINT Slot, const D3D12_VERTEX_BUFFER_VIEW& VBView)
{
    m_CommandList->IASetVertexBuffers(Slot, 1, &VBView);
}

void GraphicsContext::SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& IBView)
{
    m_CommandList->IASetIndexBuffer(&IBView);
}

void GraphicsContext::Draw(UINT VertexCount, UINT VertexStartOffset)
{
    m_CommandList->DrawInstanced(VertexCount, 1, VertexStartOffset, 0);
}

void GraphicsContext::DrawIndexed(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)
{
    m_CommandList->DrawIndexedInstanced(IndexCount, 1, StartIndexLocation, BaseVertexLocation, 0);
}