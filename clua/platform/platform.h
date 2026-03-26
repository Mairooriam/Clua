#ifndef CORE_PLATFORM_H
#define CORE_PLATFORM_H

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
    #define CORE_PLATFORM_WINDOWS
    #include "windows/wCore.h"
#else
    #error "Unsupported platform"
#endif

#endif // CORE_PLATFORM_H