#pragma once

#include "bb3d/core/Core.hpp"
#include "spdlog/spdlog.h"
#include <memory>

namespace bb3d {

    /**
     * @brief Centralized logging system based on spdlog.
     * 
     * This class provides two distinct log channels:
     * - **Core Logger**: For internal engine messages (prefix [CORE]).
     * - **Client Logger**: For user application messages (prefix [APP]).
     * 
     * @note The system is thread-safe and can be used from any thread (including JobSystem).
     */
    class Log
    {
    public:
        /**
         * @brief Initializes the logging system with default configuration.
         * 
         * Configures console output (stdout) with colors and disables file output.
         */
        static void Init();

        /**
         * @brief Initializes the logging system according to the provided configuration.
         * 
         * Allows enabling/disabling console or file logging, and setting the output directory.
         * @param config Engine configuration containing log parameters (SystemConfig).
         */
        static void Init(const struct EngineConfig& config);

        /** 
         * @brief Retrieves the engine's internal logger.
         * @return std::shared_ptr<spdlog::logger>& Reference to the Core logger pointer.
         */
        inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
        
        /** 
         * @brief Retrieves the logger for the client application.
         * @return std::shared_ptr<spdlog::logger>& Reference to the Client logger pointer.
         */
        inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }
        
        /** @brief Flushes all loggers. */
        inline static void Flush() { if (s_CoreLogger) s_CoreLogger->flush(); if (s_ClientLogger) s_ClientLogger->flush(); }

    private:
        static std::shared_ptr<spdlog::logger> s_CoreLogger;
        static std::shared_ptr<spdlog::logger> s_ClientLogger;
    };

}

// --- Engine Log Macros (CORE) ---
// Use these macros inside engine code (src/bb3d/...)
#define BB_CORE_TRACE(...)    SPDLOG_LOGGER_CALL(::bb3d::Log::GetCoreLogger(), spdlog::level::trace, __VA_ARGS__)
#define BB_CORE_DEBUG(...)    SPDLOG_LOGGER_CALL(::bb3d::Log::GetCoreLogger(), spdlog::level::debug, __VA_ARGS__)
#define BB_CORE_INFO(...)     SPDLOG_LOGGER_CALL(::bb3d::Log::GetCoreLogger(), spdlog::level::info, __VA_ARGS__)
#define BB_CORE_WARN(...)     SPDLOG_LOGGER_CALL(::bb3d::Log::GetCoreLogger(), spdlog::level::warn, __VA_ARGS__)
#define BB_CORE_ERROR(...)    SPDLOG_LOGGER_CALL(::bb3d::Log::GetCoreLogger(), spdlog::level::err, __VA_ARGS__)
#define BB_CORE_FATAL(...)    SPDLOG_LOGGER_CALL(::bb3d::Log::GetCoreLogger(), spdlog::level::critical, __VA_ARGS__)
#define BB_CORE_FLUSH()       ::bb3d::Log::Flush()

// --- Application Log Macros (CLIENT) ---
// Use these macros in game / application code
#define BB_TRACE(...)         SPDLOG_LOGGER_CALL(::bb3d::Log::GetClientLogger(), spdlog::level::trace, __VA_ARGS__)
#define BB_DEBUG(...)         SPDLOG_LOGGER_CALL(::bb3d::Log::GetClientLogger(), spdlog::level::debug, __VA_ARGS__)
#define BB_INFO(...)          SPDLOG_LOGGER_CALL(::bb3d::Log::GetClientLogger(), spdlog::level::info, __VA_ARGS__)
#define BB_WARN(...)          SPDLOG_LOGGER_CALL(::bb3d::Log::GetClientLogger(), spdlog::level::warn, __VA_ARGS__)
#define BB_ERROR(...)         SPDLOG_LOGGER_CALL(::bb3d::Log::GetClientLogger(), spdlog::level::err, __VA_ARGS__)
#define BB_FATAL(...)         SPDLOG_LOGGER_CALL(::bb3d::Log::GetClientLogger(), spdlog::level::critical, __VA_ARGS__)