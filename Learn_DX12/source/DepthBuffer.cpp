#include "../include/DepthBuffer.hpp"
#include "../include/GraphicsCore.hpp"

void DepthBuffer::Create(const std::wstring& Name, uint32_t Width, uint32_t Height, DXGI_FORMAT Format, DescriptorHeap& DSVHeap, D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr)
{
    Create(Name, Width, Height, 1, Format, DSVHeap, VidMemPtr);
}

void DepthBuffer::Create(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t Samples, DXGI_FORMAT Format, DescriptorHeap& DSVHeap, D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr)
{
    // Use Graphics::g_Device directly
    D3D12_RESOURCE_DESC ResourceDesc = DescribeTex2D(Width, Height, 1, 1, Format, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
    ResourceDesc.SampleDesc.Count = Samples;

    D3D12_CLEAR_VALUE ClearValue = {};
    ClearValue.Format = Format;
    ClearValue.DepthStencil.Depth = m_ClearDepth;
    ClearValue.DepthStencil.Stencil = m_ClearStencil;

    CreateTextureResource(Name, ResourceDesc, ClearValue, VidMemPtr);
    CreateDerivedViews(Format, DSVHeap);
}

void DepthBuffer::CreateDerivedViews(DXGI_FORMAT Format, DescriptorHeap& DSVHeap)
{
    ID3D12Resource* Resource = m_pResource.Get();

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
    dsvDesc.Format = GetDSVFormat(Format);
    if (Resource->GetDesc().SampleDesc.Count == 1)
    {
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Texture2D.MipSlice = 0;
    }
    else
    {
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
    }

    if (m_hDSV[0].ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
    {
        m_hDSV[0] = DSVHeap.Alloc(1);
        // We only allocate 1 DSV for now to save space in the small heap
        // m_hDSV[1] = DSVHeap.Alloc(1); 
    }

    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    Graphics::g_Device->CreateDepthStencilView(Resource, &dsvDesc, m_hDSV[0]);

    // Disabled read-only views for now as they require more descriptors
    /*
    dsvDesc.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH;
    Graphics::g_Device->CreateDepthStencilView(Resource, &dsvDesc, m_hDSV[1]);

    DXGI_FORMAT stencilReadFormat = GetStencilFormat(Format);
    if (stencilReadFormat != DXGI_FORMAT_UNKNOWN)
    {
        if (m_hDSV[2].ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
        {
            m_hDSV[2] = DSVHeap.Alloc(1);
            m_hDSV[3] = DSVHeap.Alloc(1);
        }

        dsvDesc.Flags = D3D12_DSV_FLAG_READ_ONLY_STENCIL;
        Graphics::g_Device->CreateDepthStencilView(Resource, &dsvDesc, m_hDSV[2]);

        dsvDesc.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH | D3D12_DSV_FLAG_READ_ONLY_STENCIL;
        Graphics::g_Device->CreateDepthStencilView(Resource, &dsvDesc, m_hDSV[3]);
    }
    else
    {
        m_hDSV[2] = m_hDSV[0];
        m_hDSV[3] = m_hDSV[1];
    }
    */

    // SRV Creation disabled for now
    /*
    if (m_hDepthSRV.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
        m_hDepthSRV = Graphics::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Create the shader resource view
    D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
    SRVDesc.Format = GetDepthFormat(Format);
    if (dsvDesc.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE2D)
    {
        SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        SRVDesc.Texture2D.MipLevels = 1;
    }
    else
    {
        SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
    }
    SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    Graphics::g_Device->CreateShaderResourceView( Resource, &SRVDesc, m_hDepthSRV );
    */
}
