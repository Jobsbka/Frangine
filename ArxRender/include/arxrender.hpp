#pragma once
#include "arxrender_types.hpp"

#define ARXRENDER_VERSION_MAJOR 0
#define ARXRENDER_VERSION_MINOR 1
#define ARXRENDER_VERSION_PATCH 0

#if defined(_WIN32)
    #ifdef ARXRENDER_EXPORTS
        #define AR_API __declspec(dllexport)
    #else
        #define AR_API __declspec(dllimport)
    #endif
#elif defined(__GNUC__) || defined(__clang__)
    #define AR_API __attribute__((visibility("default")))
#else
    #define AR_API
#endif

namespace arxrender {
struct AR_version { uint8_t major, minor, patch; };
AR_API AR_version get_version();
} // namespace arxrender