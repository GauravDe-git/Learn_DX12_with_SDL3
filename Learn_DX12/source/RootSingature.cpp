#include "../include/RootSignature.hpp"
#include "../include/GraphicsCore.hpp" // For g_Device

#include <stdexcept>

using namespace Microsoft::WRL;

RootSignature::RootSignature(uint32_t NumRootParams, uint32_t NumStaticSamplers)
    : m_Finalized(false), m_NumParameters(NumRootParams), m_NumSamplers(NumStaticSamplers)
{
    Reset(NumRootParams, NumStaticSamplers);
}

void RootSignature::Reset(uint32_t NumRootParams, uint32_t NumStaticSamplers)
{
    if (NumRootParams > 0)
        m_ParamArray.resize(NumRootParams);
    else
        m_ParamArray.clear();

    if (NumStaticSamplers > 0)
        m_SamplerArray.resize(NumStaticSamplers);
    else
        m_SamplerArray.clear();

    m_NumParameters = NumRootParams;
    m_NumSamplers = NumStaticSamplers;
    m_Finalized = false;
}

void RootSignature::InitAsConstants(uint32_t ParamIndex, uint32_t NumDwords, uint32_t Register, uint32_t RegisterSpace, D3D12_SHADER_VISIBILITY Visibility)
{
    m_ParamArray[ParamIndex].InitAsConstants(NumDwords, Register, RegisterSpace, Visibility);
}

void RootSignature::InitAsConstantBuffer(uint32_t ParamIndex, uint32_t Register, uint32_t RegisterSpace, D3D12_SHADER_VISIBILITY Visibility)
{
    m_ParamArray[ParamIndex].InitAsConstantBufferView(Register, RegisterSpace, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, Visibility);
}

void RootSignature::InitStaticSampler(uint32_t SamplerIndex, const D3D12_STATIC_SAMPLER_DESC& NonStaticSamplerDesc)
{
    m_SamplerArray[SamplerIndex] = NonStaticSamplerDesc;
}

void RootSignature::Finalize(const std::wstring& name, D3D12_ROOT_SIGNATURE_FLAGS Flags)
{
    if (m_Finalized) return;

    // Use the D3DX12 helper for the description
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC RootSigDesc;

    // This helper function handles all the pointer assignments and counts for us
    RootSigDesc.Init_1_1(m_NumParameters, m_ParamArray.data(), m_NumSamplers, m_SamplerArray.data(), Flags);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;

    // Serialize
    HRESULT hr = D3DX12SerializeVersionedRootSignature(&RootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error);
    if (FAILED(hr))
    {
        if (error)
        {
            OutputDebugStringA((const char*)error->GetBufferPointer());
        }
        throw std::runtime_error("Failed to serialize root signature");
    }

    // Create
    hr = Graphics::g_Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_Signature));
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to create root signature");
    }

    m_Signature->SetName(name.c_str());
    m_Finalized = true;
}