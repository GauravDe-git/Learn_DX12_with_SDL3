#pragma once

#include <DirectXMath.h>
#include "GpuResource.hpp"

namespace Graphics
{
    namespace Shapes
    {
        // Moved from main.cpp
        struct VertexPosColor
        {
            DirectX::XMFLOAT3 Position;
            DirectX::XMFLOAT3 Color;
        };

        // Initializes the buffers with the cube data
        void InitializeBox(GpuResource& DestVerts, GpuResource& DestIndices);
    }
}