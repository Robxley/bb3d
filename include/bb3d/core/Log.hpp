#pragma once

#include "bb3d/core/Core.hpp"
#include "spdlog/spdlog.h"
#include <memory>

namespace bb3d {

    /**
     * @brief Système de journalisation (Logging) centralisé basé sur spdlog.
     * 
     * Cette classe fournit deux canaux de log distincts :
     * - **Core Logger** : Pour les messages internes du moteur (préfixe [CORE]).
     * - **Client Logger** : Pour les messages de l'application utilisatrice (préfixe [APP]).
     * 
     * @note Le système est thread-safe et peut être utilisé depuis n'importe quel thread (JobSystem inclus).
     */
    class Log
    {
    public:
        /**
         * @brief Initialise le système de logs avec la configuration par défaut.
         * 
         * Configure la sortie console (stdout) avec couleurs et désactive la sortie fichier.
         */
        static void Init();

        /**
         * @brief Initialise le système de logs selon la configuration fournie.
         * 
         * Permet d'activer/désactiver la console ou le fichier de log, et de définir le dossier de sortie.
         * @param config Configuration du moteur contenant les paramètres de log (SystemConfig).
         */
        static void Init(const struct EngineConfig& config);

        /** 
         * @brief Récupère le logger interne du moteur.
         * @return std::shared_ptr<spdlog::logger>& Référence vers le pointeur du logger Core.
         */
        inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
        
        /** 
         * @brief Récupère le logger pour l'application cliente.
         * @return std::shared_ptr<spdlog::logger>& Référence vers le pointeur du logger Client.
         */
        inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

    private:
        static std::shared_ptr<spdlog::logger> s_CoreLogger;
        static std::shared_ptr<spdlog::logger> s_ClientLogger;
    };

}

// --- Macros de Log pour le Moteur (CORE) ---
// Utilisez ces macros à l'intérieur du code du moteur (src/bb3d/...)
#define BB_CORE_TRACE(...)    ::bb3d::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define BB_CORE_INFO(...)     ::bb3d::Log::GetCoreLogger()->info(__VA_ARGS__)
#define BB_CORE_WARN(...)     ::bb3d::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define BB_CORE_ERROR(...)    ::bb3d::Log::GetCoreLogger()->error(__VA_ARGS__)
#define BB_CORE_FATAL(...)    ::bb3d::Log::GetCoreLogger()->critical(__VA_ARGS__)

// --- Macros de Log pour l'Application (CLIENT) ---
// Utilisez ces macros dans le code du jeu / application
#define BB_TRACE(...)         ::bb3d::Log::GetClientLogger()->trace(__VA_ARGS__)
#define BB_INFO(...)          ::bb3d::Log::GetClientLogger()->info(__VA_ARGS__)
#define BB_WARN(...)          ::bb3d::Log::GetClientLogger()->warn(__VA_ARGS__)
#define BB_ERROR(...)         ::bb3d::Log::GetClientLogger()->error(__VA_ARGS__)
#define BB_FATAL(...)         ::bb3d::Log::GetClientLogger()->critical(__VA_ARGS__)