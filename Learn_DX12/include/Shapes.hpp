#pragma once

#include <DirectXMath.h>
#include "GpuBuffer.hpp"

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

        extern const uint32_t BoxIndexCount;

        // Initializes the buffers with the cube data
        void InitializeBox(GpuBuffer& DestVerts, GpuBuffer& DestIndices);
    }
}