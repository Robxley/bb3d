#include "bb3d/core/Log.hpp"
#include "bb3d/core/Config.hpp"
#include "bb3d/core/JobSystem.hpp"
#include "bb3d/core/EventBus.hpp"

#include <iostream>
#include <atomic>
#include <chrono>

// --- Event Definition ---
struct TestEvent {
    int id;
    std::string message;
};

struct PlayerDiedEvent {
    int playerId;
};

void runCoreSystemsTest(const bb3d::EngineConfig& logConfig) {
    BB_PROFILE_FRAME("MainThread");
    
    bb3d::Log::Init(logConfig);
    BB_CORE_INFO("--- Test Unitaire 08 : Core Systems ---");

    // 1. TEST JOB SYSTEM
    {
        BB_CORE_INFO("[Test] JobSystem (Work Stealing & Wait)...");
        bb3d::JobSystem jobSystem;
        jobSystem.init(); // Auto-detect threads

        // A. Test Wait & Counter
        std::atomic<int> counterValue{0};
        const int jobCount = 50;
        
        // On crée un compteur initialisé au nombre de jobs
        auto batchCounter = std::make_shared<std::atomic<int>>(jobCount);

        for (int i = 0; i < jobCount; ++i) {
            // Note: On passe le compteur à execute, qui le décrémentera automatiquement
            jobSystem.execute([&counterValue]() {
                // Travail simulé
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                counterValue++;
            }, batchCounter);
        }

        BB_CORE_INFO("Attente active des jobs...");
        jobSystem.wait(batchCounter); // Le thread principal aide ici !

        if (counterValue == jobCount) {
            BB_CORE_INFO("[Success] {} tâches terminées sans sleep arbitraire.", counterValue.load());
        } else {
            BB_CORE_ERROR("[Fail] {}/{} tâches terminées.", counterValue.load(), jobCount);
        }

        // B. Test Dispatch (Parallel For)
        std::atomic<int> dispatchSum{0};
        const int dataSize = 1000;
        const int groupSize = 100;

        BB_CORE_INFO("Dispatch sur {0} éléments...", dataSize);
        jobSystem.dispatch(dataSize, groupSize, [&](uint32_t /*index*/, uint32_t /*count*/) {
            // Simulation travail vectoriel
            dispatchSum += 1; 
        });

        if (dispatchSum == dataSize) {
            BB_CORE_INFO("[Success] Dispatch terminé. Somme = {0}", dispatchSum.load());
        } else {
            BB_CORE_ERROR("[Fail] Dispatch incorrect. Somme = {0}", dispatchSum.load());
        }

        // D. Test Exception Safe
        jobSystem.executeSafe([]() {
            BB_CORE_WARN("Job: Je vais lancer une exception (C'est prévu !)");
            throw std::runtime_error("Erreur Volontaire pour tester executeSafe avec Logging");
        });

        // E. Test Stop Token (Long Running Job)
        std::atomic<bool> longJobStarted{false};
        std::atomic<bool> longJobStopped{false};

        jobSystem.execute([&](std::stop_token st) {
            longJobStarted = true;
            BB_CORE_INFO("LongJob: Démarré.");
            while (!st.stop_requested()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            longJobStopped = true;
        });

        // Laisser le temps au job de démarrer
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        BB_CORE_INFO("Arrêt du JobSystem...");
        jobSystem.shutdown();

        if (longJobStopped) {
            BB_CORE_INFO("[Success] LongJob interrompu.");
        } else {
             BB_CORE_WARN("[Warn] LongJob status incertain (Start:{}, Stop:{}) - Possible si shutdown trop rapide.", longJobStarted.load(), longJobStopped.load());
        }
    }

    // 2. TEST EVENT BUS
    {
        BB_CORE_INFO("[Test] EventBus...");
        bb3d::EventBus eventBus;
        bool received = false;
        int receivedId = 0;

        // Abonnement
        eventBus.subscribe<TestEvent>([&](const TestEvent& e) {
            BB_CORE_INFO("Event Reçu : [{}] {}", e.id, e.message);
            received = true;
            receivedId = e.id;
        });

        // Publication
        TestEvent evt{42, "Hello EventBus"};
        eventBus.publish(evt);

        if (received && receivedId == 42) {
            BB_CORE_INFO("[Success] EventBus a correctement distribué l'événement.");
        } else {
            BB_CORE_ERROR("[Fail] EventBus n'a pas reçu l'événement.");
            throw std::runtime_error("EventBus test failed");
        }

        // Test Multi-Subscribe
        bool p1 = false, p2 = false;
        eventBus.subscribe<PlayerDiedEvent>([&](const PlayerDiedEvent&){ p1 = true; });
        eventBus.subscribe<PlayerDiedEvent>([&](const PlayerDiedEvent&){ p2 = true; });
        
        eventBus.publish(PlayerDiedEvent{1});
        
        if (p1 && p2) BB_CORE_INFO("[Success] EventBus : Multi-subscriber OK.");
        else BB_CORE_ERROR("[Fail] EventBus : Multi-subscriber échoué.");

        // Test Queue (Différé)
        bool queuedReceived = false;
        eventBus.subscribe<std::string>([&](const std::string& msg) {
            BB_CORE_INFO("Event Différé Reçu : {}", msg);
            queuedReceived = true;
        });

        eventBus.enqueue(std::string("Je suis en retard !"));
        
        if (queuedReceived) {
            BB_CORE_ERROR("[Fail] L'événement différé a été traité trop tôt !");
        }

        BB_CORE_INFO("Dispatching queue...");
        eventBus.dispatchQueued();

        if (queuedReceived) {
            BB_CORE_INFO("[Success] EventBus : Queue dispatch OK.");
        } else {
            BB_CORE_ERROR("[Fail] EventBus : Queue dispatch échoué.");
        }
    }
}

int main() {
    bb3d::EngineConfig logConfig;
    logConfig.system.logDirectory = "unit_test_logs";
    logConfig.system.logFileName = "unit_test_08.log";
    
    try {
        runCoreSystemsTest(logConfig);
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}