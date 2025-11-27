#include "../include/Shapes.hpp"
#include "../include/GraphicsCore.hpp"
#include "../include/CommandContext.hpp"
#include <d3dx12.h>

using namespace DirectX;

namespace Graphics
{
    namespace Shapes
    {
        void InitializeBox(GpuResource& DestVerts, GpuResource& DestIndices)
        {
            // Cube Data (Moved from main.cpp)
            static VertexPosColor g_Vertices[8] = {
                { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) }, // 0
                { XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) }, // 1
                { XMFLOAT3(1.0f,  1.0f, -1.0f), XMFLOAT3(1.0f, 1.0f, 0.0f) }, // 2
                { XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) }, // 3
                { XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) }, // 4
                { XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT3(0.0f, 1.0f, 1.0f) }, // 5
                { XMFLOAT3(1.0f,  1.0f,  1.0f), XMFLOAT3(1.0f, 1.0f, 1.0f) }, // 6
                { XMFLOAT3(1.0f, -1.0f,  1.0f), XMFLOAT3(1.0f, 0.0f, 1.0f) }  // 7
            };

            static WORD g_Indicies[36] =
            {
                0, 1, 2, 0, 2, 3,
                4, 6, 5, 4, 7, 6,
                4, 5, 1, 4, 1, 0,
                3, 2, 6, 3, 6, 7,
                1, 5, 6, 1, 6, 2,
                4, 0, 3, 4, 3, 7
            };

            // 1. Create Resources (Default Heap)
            // We create the committed resources here.
            {
                auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
                auto desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(g_Vertices));
                Graphics::g_Device->CreateCommittedResource(
                    &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                    D3D12_RESOURCE_STATE_COMMON, nullptr,
                    IID_PPV_ARGS(DestVerts.GetAddressOf()));
                DestVerts.SetUsageState(D3D12_RESOURCE_STATE_COMMON);
            }

            {
                auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
                auto desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(g_Indicies));
                Graphics::g_Device->CreateCommittedResource(
                    &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                    D3D12_RESOURCE_STATE_COMMON, nullptr,
                    IID_PPV_ARGS(DestIndices.GetAddressOf()));
                DestIndices.SetUsageState(D3D12_RESOURCE_STATE_COMMON);
            }

            // 2. Upload Data
            // Create a temporary context just for this upload
            GraphicsContext Context(Graphics::g_CommandQueue);

            Context.InitializeBuffer(DestVerts, g_Vertices, sizeof(g_Vertices));
            Context.InitializeBuffer(DestIndices, g_Indicies, sizeof(g_Indicies));

            Context.Finish(true); // Wait for upload to complete
        }
    }
}