#include "../include/PipelineState.hpp"
#include "../include/RootSignature.hpp"
#include "../include/GraphicsCore.hpp"

#include <stdexcept>

GraphicsPipelineState::GraphicsPipelineState()
{
    // Initialize the description with default values
    ZeroMemory(&m_PSODesc, sizeof(m_PSODesc));
    m_PSODesc.NodeMask = 1;
    m_PSODesc.SampleMask = 0xFFFFFFFFu;
    m_PSODesc.SampleDesc.Count = 1;
    m_PSODesc.InputLayout.NumElements = 0;
    m_PSODesc.InputLayout.pInputElementDescs = nullptr;
    m_PSODesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    m_PSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    m_PSODesc.NumRenderTargets = 0;
    m_PSODesc.DSVFormat = DXGI_FORMAT_UNKNOWN;

    // Set sensible defaults for states using CD3DX12 helpers
    m_PSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    m_PSODesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    m_PSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
}

void GraphicsPipelineState::SetRootSignature(const RootSignature& BindMappings)
{
    m_PSODesc.pRootSignature = BindMappings.GetSignature();
}

void GraphicsPipelineState::SetVertexShader(const void* Binary, size_t Size)
{
    m_PSODesc.VS = CD3DX12_SHADER_BYTECODE(Binary, Size);
}

void GraphicsPipelineState::SetPixelShader(const void* Binary, size_t Size)
{
    m_PSODesc.PS = CD3DX12_SHADER_BYTECODE(Binary, Size);
}

void GraphicsPipelineState::SetInputLayout(const std::vector<D3D12_INPUT_ELEMENT_DESC>& InputElementDescs)
{
    // We must copy the vector because the struct only holds a pointer!
    m_InputLayouts = InputElementDescs;
    m_PSODesc.InputLayout.NumElements = (UINT)m_InputLayouts.size();
    m_PSODesc.InputLayout.pInputElementDescs = m_InputLayouts.data();
}

void GraphicsPipelineState::SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE TopologyType)
{
    m_PSODesc.PrimitiveTopologyType = TopologyType;
}

void GraphicsPipelineState::SetRenderTargetFormats(uint32_t NumRTVs, const DXGI_FORMAT* RTVFormats, DXGI_FORMAT DSVFormat, UINT MsaaCount, UINT MsaaQuality)
{
    for (uint32_t i = 0; i < NumRTVs; ++i)
    {
        m_PSODesc.RTVFormats[i] = RTVFormats[i];
    }
    for (uint32_t i = NumRTVs; i < 8; ++i)
    {
        m_PSODesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
    }
    m_PSODesc.NumRenderTargets = NumRTVs;
    m_PSODesc.DSVFormat = DSVFormat;
    m_PSODesc.SampleDesc.Count = MsaaCount;
    m_PSODesc.SampleDesc.Quality = MsaaQuality;
}

void GraphicsPipelineState::SetRenderTargetFormat(DXGI_FORMAT RTVFormat, DXGI_FORMAT DSVFormat, UINT MsaaCount, UINT MsaaQuality)
{
    SetRenderTargetFormats(1, &RTVFormat, DSVFormat, MsaaCount, MsaaQuality);
}

void GraphicsPipelineState::SetBlendState(const D3D12_BLEND_DESC& BlendDesc)
{
    m_PSODesc.BlendState = BlendDesc;
}

void GraphicsPipelineState::SetRasterizerState(const D3D12_RASTERIZER_DESC& RasterizerDesc)
{
    m_PSODesc.RasterizerState = RasterizerDesc;
}

void GraphicsPipelineState::SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& DepthStencilDesc)
{
    m_PSODesc.DepthStencilState = DepthStencilDesc;
}

void GraphicsPipelineState::Finalize(const std::wstring& Name)
{
    HRESULT hr = Graphics::g_Device->CreateGraphicsPipelineState(&m_PSODesc, IID_PPV_ARGS(&m_PSO));
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to create Graphics Pipeline State");
    }
    m_PSO->SetName(Name.c_str());
}