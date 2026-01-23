#include "bb3d/core/Log.hpp"
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

void runCoreSystemsTest() {
    BB_PROFILE_FRAME("MainThread");
    
    bb3d::Log::Init();
    BB_CORE_INFO("--- Test Unitaire 03 : Core Systems ---");

    // 1. TEST JOB SYSTEM
    {
        BB_CORE_INFO("[Test] JobSystem...");
        bb3d::JobSystem jobSystem;
        jobSystem.init(); // Auto-detect threads

        // A. Test Basique (Compteur)
        std::atomic<int> counter{0};
        const int jobCount = 50;

        for (int i = 0; i < jobCount; ++i) {
            jobSystem.execute([&counter]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                counter++;
            });
        }

        // B. Test Exception Safe
        jobSystem.executeSafe([]() {
            BB_CORE_WARN("Job: Je vais lancer une exception (C'est prévu !)");
            throw std::runtime_error("Erreur Volontaire pour tester executeSafe");
        });

        // C. Test Stop Token (Long Running Job)
        std::atomic<bool> longJobStarted{false};
        std::atomic<bool> longJobStopped{false};

        jobSystem.execute([&](std::stop_token st) {
            longJobStarted = true;
            BB_CORE_INFO("LongJob: Démarré, attente stop...");
            while (!st.stop_requested()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            BB_CORE_INFO("LongJob: Stop détecté !");
            longJobStopped = true;
        });

        // Attente fin jobs normaux
        int attempts = 0;
        while (counter < jobCount && attempts < 200) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        
        // Vérification Basique
        if (counter == jobCount) {
            BB_CORE_INFO("[Success] {} tâches terminées.", counter.load());
        } else {
            BB_CORE_ERROR("[Fail] {}/{} tâches terminées.", counter.load(), jobCount);
        }

        // Vérification Stop Token
        // On demande l'arrêt du système, ce qui doit annuler le LongJob
        BB_CORE_INFO("Arrêt du JobSystem...");
        jobSystem.shutdown();

        if (longJobStarted && longJobStopped) {
            BB_CORE_INFO("[Success] LongJob correctement interrompu par le stop_token.");
        } else {
            BB_CORE_ERROR("[Fail] LongJob non géré correctement (Start:{}, Stop:{})", longJobStarted.load(), longJobStopped.load());
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
    }
}

int main() {
    try {
        runCoreSystemsTest();
    } catch (const std::exception& e) {
        std::cerr << "Test Failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
