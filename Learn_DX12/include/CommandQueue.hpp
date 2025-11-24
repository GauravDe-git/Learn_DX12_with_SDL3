#pragma once

#include "CommandAllocatorPool.hpp"

#include <wrl.h>

class CommandQueue
{
public:
    CommandQueue(D3D12_COMMAND_LIST_TYPE Type);
    ~CommandQueue();

    void Create(ID3D12Device* pDevice);
    void Shutdown();

    bool IsReady() const { return m_CommandQueue != nullptr; }

    uint64_t IncrementFence();
    bool IsFenceComplete(uint64_t FenceValue);
    void WaitForFence(uint64_t FenceValue);
    void WaitForIdle() { WaitForFence(IncrementFence()); }

    ID3D12CommandQueue* GetCommandQueue() { return m_CommandQueue.Get(); }
    uint64_t GetNextFenceValue() { return m_NextFenceValue; }
    uint64_t GetLastCompletedFenceValue() { return m_LastCompletedFenceValue; }

    // Execute a command list.
    // Returns the fence value to wait for for this command list.
    uint64_t ExecuteCommandList(ID3D12CommandList* List);

    ID3D12CommandAllocator* RequestAllocator();
    void DiscardAllocator(uint64_t FenceValueForReset, ID3D12CommandAllocator* Allocator);

private:
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_CommandQueue;
    const D3D12_COMMAND_LIST_TYPE m_Type;

    CommandAllocatorPool m_AllocatorPool;
    std::mutex m_FenceMutex;
    std::mutex m_EventMutex;

    Microsoft::WRL::ComPtr<ID3D12Fence> m_pFence;
    uint64_t m_NextFenceValue;
    uint64_t m_LastCompletedFenceValue;
    HANDLE m_FenceEventHandle;
};