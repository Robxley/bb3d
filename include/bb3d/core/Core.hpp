#pragma once

#include "bb3d/core/Base.hpp"
#include <utility>

// Define a consistent namespace for the engine
#define BB3D_NAMESPACE bb3d

namespace BB3D_NAMESPACE
{
    // Fundamental types are now in Base.hpp
}

// Profiling macros (integration with Tracy)
// These will be empty if profiling is disabled
#if defined(BB3D_ENABLE_PROFILING)
    #include "tracy/Tracy.hpp"
    #define BB_PROFILE_SCOPE(name) ZoneScopedN(name)
    #define BB_PROFILE_FRAME(name) FrameMarkNamed(name)
#else
    #define BB_PROFILE_SCOPE(name)
    #define BB_PROFILE_FRAME(name)
#endif

// Basic assert for debugging
#if defined(BB3D_DEBUG)
    #define BB_ASSERT(x, ...) { if(!(x)) { BB_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
    #define BB_CORE_ASSERT(x, ...) { if(!(x)) { BB_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#else
    #define BB_ASSERT(x, ...)
    #define BB_CORE_ASSERT(x, ...)
#endif
