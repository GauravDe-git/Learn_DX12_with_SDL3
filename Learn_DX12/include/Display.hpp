#pragma once

#include <cstdint>

#include "ColorBuffer.hpp"

namespace Display
{
    // Initialize the swap chain and display buffers
    void Initialize(void);

    // Cleanup resources
    void Shutdown(void);

    // Handle window resizing
    void Resize(uint32_t width, uint32_t height);

    // Present the current frame and wait for the next buffer
    void Present(void);

    // Get the ColorBuffer for the current back buffer
    ColorBuffer& GetCurrentBuffer(void);

    // Get the current window dimensions
    uint32_t GetWidth(void);
    uint32_t GetHeight(void);
}