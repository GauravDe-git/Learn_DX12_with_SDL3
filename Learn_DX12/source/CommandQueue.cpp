#include "../include/CommandQueue.hpp"

#include <cassert>

#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif
#include <algorithm>

CommandQueue::CommandQueue(D3D12_COMMAND_LIST_TYPE Type) :
    m_Type(Type),
    m_CommandQueue(nullptr),
    m_pFence(nullptr),
    m_NextFenceValue((uint64_t)Type << 56 | 1),
    m_LastCompletedFenceValue((uint64_t)Type << 56),
    m_AllocatorPool(Type),
    m_FenceEventHandle(nullptr)
{
}

CommandQueue::~CommandQueue()
{
    Shutdown();
}

void CommandQueue::Shutdown()
{
    if (m_CommandQueue == nullptr)
        return;

    m_AllocatorPool.Shutdown();

    if (m_FenceEventHandle != nullptr)
    {
        CloseHandle(m_FenceEventHandle);
        m_FenceEventHandle = nullptr;
    }

    m_pFence = nullptr;
    m_CommandQueue = nullptr;
}

void CommandQueue::Create(ID3D12Device* pDevice)
{
    assert(pDevice != nullptr);
    assert(!IsReady());
    assert(m_AllocatorPool.Size() == 0);

    D3D12_COMMAND_QUEUE_DESC QueueDesc = {};
    QueueDesc.Type = m_Type;
    QueueDesc.NodeMask = 1;
    pDevice->CreateCommandQueue(&QueueDesc, IID_PPV_ARGS(&m_CommandQueue));
    m_CommandQueue->SetName(L"CommandQueue::m_CommandQueue");

    pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pFence));
    m_pFence->SetName(L"CommandQueue::m_pFence");
    m_pFence->Signal((uint64_t)m_Type << 56);

    m_FenceEventHandle = CreateEvent(nullptr, false, false, nullptr);
    assert(m_FenceEventHandle != NULL);

    m_AllocatorPool.Create(pDevice);

    assert(IsReady());
}

uint64_t CommandQueue::ExecuteCommandList(ID3D12CommandList* List)
{
    std::lock_guard<std::mutex> LockGuard(m_FenceMutex);

    // DELETE OR COMMENT OUT THIS LINE for now:
    // ((ID3D12GraphicsCommandList*)List)->Close(); 

    // Kickoff the command list
    ID3D12CommandList* const ppCommandLists[] = { List };
    m_CommandQueue->ExecuteCommandLists(1, ppCommandLists);

    // Signal the next fence value (with the GPU)
    m_CommandQueue->Signal(m_pFence.Get(), m_NextFenceValue);

    // And increment the fence value.  
    return m_NextFenceValue++;
}

uint64_t CommandQueue::IncrementFence(void)
{
    std::lock_guard<std::mutex> LockGuard(m_FenceMutex);
    m_CommandQueue->Signal(m_pFence.Get(), m_NextFenceValue);
    return m_NextFenceValue++;
}

bool CommandQueue::IsFenceComplete(uint64_t FenceValue)
{
    // Avoid querying the fence value by testing against the last one seen.
    // The max() is to protect against an unlikely race condition that could cause the last
    // completed fence value to regress.
    if (FenceValue > m_LastCompletedFenceValue)
        m_LastCompletedFenceValue = std::max(m_LastCompletedFenceValue, m_pFence->GetCompletedValue());

    return FenceValue <= m_LastCompletedFenceValue;
}

void CommandQueue::WaitForFence(uint64_t FenceValue)
{
    if (IsFenceComplete(FenceValue))
        return;

    {
        std::lock_guard<std::mutex> LockGuard(m_EventMutex);

        m_pFence->SetEventOnCompletion(FenceValue, m_FenceEventHandle);
        WaitForSingleObject(m_FenceEventHandle, INFINITE);
        m_LastCompletedFenceValue = FenceValue;
    }
}

ID3D12CommandAllocator* CommandQueue::RequestAllocator()
{
    uint64_t CompletedFence = m_pFence->GetCompletedValue();
    return m_AllocatorPool.RequestAllocator(CompletedFence);
}

void CommandQueue::DiscardAllocator(uint64_t FenceValue, ID3D12CommandAllocator* Allocator)
{
    m_AllocatorPool.DiscardAllocator(FenceValue, Allocator);
}