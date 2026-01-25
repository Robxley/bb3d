#pragma once

#include "bb3d/core/Base.hpp"
#include <utility>

// Namespace global pour le moteur
#define BB3D_NAMESPACE bb3d

namespace BB3D_NAMESPACE
{
    // Types fondamentaux via Base.hpp
}

/**
 * @name Macros de Profiling
 * @{
 */
#if defined(BB_PROFILE)
    #include "tracy/Tracy.hpp"
    #define BB_PROFILE_SCOPE(name) ZoneScopedN(name)
    #define BB_PROFILE_FRAME(name) FrameMarkNamed(name)
#else
    #define BB_PROFILE_SCOPE(name)
    #define BB_PROFILE_FRAME(name)
#endif
/** @} */

/**
 * @name Macros de Debug & Assertions
 * @{
 */
#if defined(BB3D_DEBUG)
    #if defined(_MSC_VER)
        #define BB_DEBUGBREAK() __debugbreak()
    #else
        #include <signal.h>
        #define BB_DEBUGBREAK() raise(SIGTRAP)
    #endif

    #define BB_ASSERT(x, ...) { if(!(x)) { BB_ERROR("Assertion Failed: {0}", __VA_ARGS__); BB_DEBUGBREAK(); } }
    #define BB_CORE_ASSERT(x, ...) { if(!(x)) { BB_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); BB_DEBUGBREAK(); } }
#else
    #define BB_ASSERT(x, ...)
    #define BB_CORE_ASSERT(x, ...)
    #define BB_DEBUGBREAK()
#endif
/** @} */