#pragma once
#include <d3d12.h>
#include <wrl/client.h>

#include "d3dx12.h" 

#include <string>
#include <vector>

// Forward declarations
class RootSignature;
namespace Graphics { extern Microsoft::WRL::ComPtr<ID3D12Device2> g_Device; }

class GraphicsPipelineState
{
public:
    GraphicsPipelineState();
    ~GraphicsPipelineState() { Destroy(); }

    // Setup Methods - These set fields in the Description
    void SetRootSignature(const RootSignature& BindMappings);
    void SetVertexShader(const void* Binary, size_t Size);
    void SetPixelShader(const void* Binary, size_t Size);
    void SetInputLayout(const std::vector<D3D12_INPUT_ELEMENT_DESC>& InputElementDescs);
    void SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE TopologyType);

    // Format Setup
    void SetRenderTargetFormats(uint32_t NumRTVs, const DXGI_FORMAT* RTVFormats, DXGI_FORMAT DSVFormat, UINT MsaaCount = 1, UINT MsaaQuality = 0);
    void SetRenderTargetFormat(DXGI_FORMAT RTVFormat, DXGI_FORMAT DSVFormat, UINT MsaaCount = 1, UINT MsaaQuality = 0);

    // Default State Setters (Optional overrides)
    void SetBlendState(const D3D12_BLEND_DESC& BlendDesc);
    void SetRasterizerState(const D3D12_RASTERIZER_DESC& RasterizerDesc);
    void SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& DepthStencilDesc);

    // Finalize: Compiles the PSO
    void Finalize(const std::wstring& Name);

    ID3D12PipelineState* GetPipelineStateObject() const { return m_PSO.Get(); }
    void Destroy() { m_PSO.Reset(); }

private:
    // The description struct we are building up
    D3D12_GRAPHICS_PIPELINE_STATE_DESC m_PSODesc;

    // The compiled PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PSO;

    // We need to keep a copy of the Input Layout elements because the DESC only holds a pointer
    std::vector<D3D12_INPUT_ELEMENT_DESC> m_InputLayouts;
};