#pragma once

#include <memory>
#include <utility>

// Define a consistent namespace for the engine
#define BB3D_NAMESPACE bb3d

namespace BB3D_NAMESPACE
{
    // Smart pointer aliases as defined in the project specifications
    template<typename T>
    using Scope = std::unique_ptr<T>;

    template<typename T>
    using Ref = std::shared_ptr<T>;

    template<typename T, typename ... Args>
    constexpr Ref<T> CreateRef(Args&& ... args)
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

    template<typename T, typename ... Args>
    constexpr Scope<T> CreateScope(Args&& ... args)
    {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }
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
