#pragma once
#include <d3d12.h>
#include <wrl/client.h>

#include "d3dx12.h"

#include <vector>
#include <string>

// Forward declaration
namespace Graphics { extern Microsoft::WRL::ComPtr<ID3D12Device2> g_Device; }

class RootSignature
{
public:
    RootSignature(uint32_t NumRootParams = 0, uint32_t NumStaticSamplers = 0);
    ~RootSignature() { Destroy(); }

    // Reset the signature to empty
    void Reset(uint32_t NumRootParams, uint32_t NumStaticSamplers = 0);

    // Initialize a specific parameter as a set of constants (e.g.,MVP matrix)
    // Register: The shader register (e.g., b0 for cbuffer)
    // NumDwords: Size in 32-bit values (e.g., Matrix4x4 = 16 floats = 16 Dwords)
    void InitAsConstants(uint32_t ParamIndex, uint32_t NumDwords, uint32_t Register, uint32_t RegisterSpace = 0, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL);

    // Initialize as a Constant Buffer View (CBV)
    void InitAsConstantBuffer(uint32_t ParamIndex, uint32_t Register, uint32_t RegisterSpace = 0, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL);

    // Initialize a static sampler
    void InitStaticSampler(uint32_t SamplerIndex, const D3D12_STATIC_SAMPLER_DESC& NonStaticSamplerDesc);

    // Compile and create the Root Signature object
    void Finalize(const std::wstring& name, D3D12_ROOT_SIGNATURE_FLAGS Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE);

    ID3D12RootSignature* GetSignature() const { return m_Signature.Get(); }

    void Destroy() { m_Signature.Reset(); }

protected:
    bool m_Finalized;
    uint32_t m_NumParameters;
    uint32_t m_NumSamplers;

    // Using the D3DX12 helper structures for convenience
    std::vector<CD3DX12_ROOT_PARAMETER1> m_ParamArray;
    std::vector<D3D12_STATIC_SAMPLER_DESC> m_SamplerArray;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_Signature;
};
