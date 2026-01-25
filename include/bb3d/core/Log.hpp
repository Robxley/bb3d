#pragma once

#include "bb3d/core/Core.hpp"
#include "spdlog/spdlog.h"
#include <memory>

namespace bb3d {

    /**
     * @brief Système de journalisation (Logging) basé sur spdlog.
     * 
     * Fournit des loggers séparés pour le moteur (Core) et l'application (Client).
     */
    class Log
    {
    public:
        /**
         * @brief Initialise le système de logs avec les valeurs par défaut.
         */
        static void Init();

        /**
         * @brief Initialise le système de logs selon la configuration fournie.
         */
        static void Init(const struct EngineConfig& config);

        /** @brief Récupère le logger interne du moteur. */
        inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
        
        /** @brief Récupère le logger pour l'application cliente. */
        inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

    private:
        static std::shared_ptr<spdlog::logger> s_CoreLogger;
        static std::shared_ptr<spdlog::logger> s_ClientLogger;
    };

}

// --- Macros de Log pour le Moteur (CORE) ---
#define BB_CORE_TRACE(...)    ::bb3d::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define BB_CORE_INFO(...)     ::bb3d::Log::GetCoreLogger()->info(__VA_ARGS__)
#define BB_CORE_WARN(...)     ::bb3d::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define BB_CORE_ERROR(...)    ::bb3d::Log::GetCoreLogger()->error(__VA_ARGS__)
#define BB_CORE_FATAL(...)    ::bb3d::Log::GetCoreLogger()->critical(__VA_ARGS__)

// --- Macros de Log pour l'Application (CLIENT) ---
#define BB_TRACE(...)         ::bb3d::Log::GetClientLogger()->trace(__VA_ARGS__)
#define BB_INFO(...)          ::bb3d::Log::GetClientLogger()->info(__VA_ARGS__)
#define BB_WARN(...)          ::bb3d::Log::GetClientLogger()->warn(__VA_ARGS__)
#define BB_ERROR(...)         ::bb3d::Log::GetClientLogger()->error(__VA_ARGS__)
#define BB_FATAL(...)         ::bb3d::Log::GetClientLogger()->critical(__VA_ARGS__)