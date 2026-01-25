#include "bb3d/core/Log.hpp"
#include "bb3d/core/Config.hpp"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/dist_sink.h"
#include <filesystem>

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
        auto dist_sink = std::make_shared<spdlog::sinks::dist_sink_mt>();

        // 1. Console Sink
        if (config.system.logConsole) {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_pattern("%^[%T] %n: %v%$");
            dist_sink->add_sink(console_sink);
        }

        // 2. File Sink
        if (config.system.logFile) {
            try {
                if (!std::filesystem::exists(config.system.logDirectory)) {
                    std::filesystem::create_directories(config.system.logDirectory);
                }
                
                std::string logPath = config.system.logDirectory + "/bb3d.log";
                auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath, true);
                file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v");
                dist_sink->add_sink(file_sink);
            } catch (const std::exception& e) {
                // On ne peut pas logger via spdlog ici si ça échoue, on utilise stderr
                fprintf(stderr, "Failed to initialize file logging: %s\n", e.what());
            }
        }

        // Create loggers using the distribution sink
        s_CoreLogger = std::make_shared<spdlog::logger>("CORE", dist_sink);
        s_CoreLogger->set_level(spdlog::level::trace);
        s_CoreLogger->flush_on(spdlog::level::trace);

        s_ClientLogger = std::make_shared<spdlog::logger>("APP", dist_sink);
        s_ClientLogger->set_level(spdlog::level::trace);
        s_ClientLogger->flush_on(spdlog::level::trace);

        spdlog::register_logger(s_CoreLogger);
        spdlog::register_logger(s_ClientLogger);

        BB_CORE_INFO("Logging System Initialized (Console: {}, File: {})", 
            config.system.logConsole ? "ON" : "OFF", 
            config.system.logFile ? "ON" : "OFF");
    }

} // namespace bb3d
