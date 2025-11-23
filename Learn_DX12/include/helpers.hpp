#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h> // For HRESULT

#include <exception> 

// From DXSampleHelper.h 
// Source: https://github.com/Microsoft/DirectX-Graphics-Samples
inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        // can make this more complex later, but for now,
        // throwing a basic exception is perfect.
        throw std::exception("DirectX Function Failed");
    }
}