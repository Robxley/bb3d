#include "bb3d/core/Log.hpp"
#include "bb3d/core/Config.hpp"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/dist_sink.h"
#include <filesystem>
#include <algorithm>

namespace bb3d {

    std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
    std::shared_ptr<spdlog::logger> Log::s_ClientLogger;

    void Log::Init()
    {
        EngineConfig defaultConfig;
        Init(defaultConfig);
    }

    void Log::Init(const EngineConfig& config)
    {
        // On utilise un 'dist_sink' pour envoyer les logs vers plusieurs destinations (Console, Fichier, etc.)
        // de manière thread-safe.
        auto dist_sink = std::make_shared<spdlog::sinks::dist_sink_mt>();

        // 1. Console Sink (Sortie standard colorée)
        if (config.system.logConsole) {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            // Pattern: [Heure] NomLogger: Message
            console_sink->set_pattern("%^[%T] %n: %v%$");
            dist_sink->add_sink(console_sink);
        }

        // 2. File Sink (Fichier texte)
        if (config.system.logFile) {
            try {
                // Création du dossier de logs s'il n'existe pas
                if (!std::filesystem::exists(config.system.logDirectory)) {
                    std::filesystem::create_directories(config.system.logDirectory);
                }
                
                std::string logPath = config.system.logDirectory + "/bb3d.log";
                // basic_file_sink_mt est thread-safe. Le paramètre 'true' écrase le fichier à chaque démarrage (truncate).
                auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath, true);
                // Pattern détaillé pour le fichier: [Date Heure] [Nom] [Level] Message
                file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v");
                dist_sink->add_sink(file_sink);
            } catch (const std::exception& e) {
                // Fallback critique : On ne peut pas logger via spdlog ici si l'init échoue, on utilise stderr
                fprintf(stderr, "Failed to initialize file logging: %s\n", e.what());
            }
        }

        // Création des loggers utilisant le sink distribué
        // CORE : Logger interne du moteur
        s_CoreLogger = std::make_shared<spdlog::logger>("CORE", dist_sink);
        
        // APP : Logger pour le client
        s_ClientLogger = std::make_shared<spdlog::logger>("APP", dist_sink);

        // Définition du niveau de log depuis la config
        spdlog::level::level_enum level = spdlog::level::info; // Par défaut
        std::string levelStr = config.system.logLevel;
        std::transform(levelStr.begin(), levelStr.end(), levelStr.begin(), ::tolower);

        if (levelStr == "trace") level = spdlog::level::trace;
        else if (levelStr == "debug") level = spdlog::level::debug;
        else if (levelStr == "info") level = spdlog::level::info;
        else if (levelStr == "warn") level = spdlog::level::warn;
        else if (levelStr == "error") level = spdlog::level::err;
        else if (levelStr == "fatal") level = spdlog::level::critical;

        s_CoreLogger->set_level(level);
        s_CoreLogger->flush_on(level); 

        s_ClientLogger->set_level(level);
        s_ClientLogger->flush_on(level);

        // Enregistrement global pour accès via spdlog::get("CORE") si besoin
        spdlog::register_logger(s_CoreLogger);
        spdlog::register_logger(s_ClientLogger);

        BB_CORE_INFO("Logging System Initialized (Level: {0}, Console: {1}, File: {2})", 
            config.system.logLevel,
            config.system.logConsole ? "ON" : "OFF", 
            config.system.logFile ? "ON" : "OFF");
    }

} // namespace bb3d
