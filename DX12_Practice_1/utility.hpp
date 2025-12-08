#pragma once
#include <SDL3/SDL_log.h>
//#include <winerror.h> // Helper for HRESULT (FAILED macro)

// Check if we are in Release or Debug mode
#ifdef NDEBUG
    // In Release builds, this macro does nothing (compiles away)
#define ASSERT_SUCCEEDED(hr, ...) (void)(hr)
#else
    // In Debug builds, check result -> Log Error -> Crash/Break
#define ASSERT_SUCCEEDED(hr, ...) \
        if (FAILED(hr)) { \
            /* 1. Log the crash details using SDL */ \
            SDL_LogError(SDL_LOG_CATEGORY_RENDER, \
                "\n[DX12 CRASH] HRESULT Failed: 0x%08X\n" \
                "    -> File: %s\n" \
                "    -> Line: %d\n" \
                "    -> Msg:  " __VA_ARGS__ "\n", \
                (unsigned int)hr, __FILE__, __LINE__); \
            \
            /* 2. Freeze the debugger here */ \
            __debugbreak(); \
        }
#endif