#include "../include/Shapes.hpp"
#include "../include/GraphicsCore.hpp"

#include <d3dx12.h>

using namespace DirectX;

namespace Graphics
{
    namespace Shapes
    {
        const uint32_t BoxIndexCount = 36;

        void InitializeBox(GpuBuffer& DestVerts, GpuBuffer& DestIndices)
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

            DestVerts.Create(L"Box Vertices", 8, sizeof(VertexPosColor), g_Vertices);
            DestIndices.Create(L"Box Indices", 36, sizeof(WORD), g_Indicies);
        }
    }
}