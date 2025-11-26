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

    // Present the current frame
    // enableVSync: true to wait for VSync, false to present immediately
    // allowTearing: true to allow screen tearing (for lowest latency when VSync is off)
    void Present(bool enableVSync, bool allowTearing);

    // Get the ColorBuffer for the current back buffer
    ColorBuffer& GetCurrentBuffer(void);

    // Get the current window dimensions
    uint32_t GetWidth(void);
    uint32_t GetHeight(void);

    bool CheckTearingSupport();
}