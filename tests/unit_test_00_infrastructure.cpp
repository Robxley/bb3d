#include "bb3d/core/Log.hpp"
#include "bb3d/core/Config.hpp"
#include <iostream>
#include "bb3d/core/Core.hpp"
#include <chrono>
#include <thread>

int main() {
    bb3d::EngineConfig logConfig;
    logConfig.system.logDirectory = "unit_test_logs";
    bb3d::Log::Init(logConfig);
    
    BB_CORE_INFO("Validation de l'infrastructure Core...");
    BB_INFO("Validation de l'infrastructure Client (App)...");

    // Simulation d'une charge de travail pour voir le bloc dans Tracy
    BB_CORE_TRACE("Simulation d'une tâche de 100ms...");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    BB_CORE_INFO("Test d'infrastructure terminé avec succès.");
    
    return 0;
}
