#pragma once

#include "bb3d/core/Core.hpp"
#include "spdlog/spdlog.h"
#include <memory>

namespace bb3d {

    class Log
    {
    public:
        static void Init();

        inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
        inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

    private:
        static std::shared_ptr<spdlog::logger> s_CoreLogger;
        static std::shared_ptr<spdlog::logger> s_ClientLogger;
    };

}

// Core log macros
#define BB_CORE_TRACE(...)    ::bb3d::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define BB_CORE_INFO(...)     ::bb3d::Log::GetCoreLogger()->info(__VA_ARGS__)
#define BB_CORE_WARN(...)     ::bb3d::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define BB_CORE_ERROR(...)    ::bb3d::Log::GetCoreLogger()->error(__VA_ARGS__)
#define BB_CORE_FATAL(...)    ::bb3d::Log::GetCoreLogger()->critical(__VA_ARGS__)

// Client log macros
#define BB_TRACE(...)         ::bb3d::Log::GetClientLogger()->trace(__VA_ARGS__)
#define BB_INFO(...)          ::bb3d::Log::GetClientLogger()->info(__VA_ARGS__)
#define BB_WARN(...)          ::bb3d::Log::GetClientLogger()->warn(__VA_ARGS__)
#define BB_ERROR(...)         ::bb3d::Log::GetClientLogger()->error(__VA_ARGS__)
#define BB_FATAL(...)         ::bb3d::Log::GetClientLogger()->critical(__VA_ARGS__)
