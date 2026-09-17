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
    BB_CORE_INFO("--- Unit Test 08 : Core Systems ---");

    // 1. TEST JOB SYSTEM
    {
        BB_CORE_INFO("[Test] JobSystem (Work Stealing & Wait)...");
        bb3d::JobSystem jobSystem;
        jobSystem.init(); // Auto-detect worker threads

        // A. Test Wait & Counter
        std::atomic<int> counterValue{0};
        const int jobCount = 50;
        
        auto batchCounter = std::make_shared<std::atomic<int>>(jobCount);

        for (int i = 0; i < jobCount; ++i) {
            jobSystem.execute([&counterValue]() {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                counterValue++;
            }, batchCounter);
        }

        BB_CORE_INFO("Active waiting for batch jobs...");
        jobSystem.wait(batchCounter); // Main thread assists

        if (counterValue == jobCount) {
            BB_CORE_INFO("[Success] {} tasks completed.", counterValue.load());
        } else {
            BB_CORE_ERROR("[Fail] {}/{} tasks completed.", counterValue.load(), jobCount);
        }

        // B. Test Dispatch (Parallel For)
        std::atomic<int> dispatchSum{0};
        const int dataSize = 1000;
        const int groupSize = 100;

        BB_CORE_INFO("Dispatch across {0} elements...", dataSize);
        jobSystem.dispatch(dataSize, groupSize, [&](uint32_t /*index*/, uint32_t /*count*/) {
            dispatchSum += 1; 
        });

        if (dispatchSum == dataSize) {
            BB_CORE_INFO("[Success] Dispatch complete. Sum = {0}", dispatchSum.load());
        } else {
            BB_CORE_ERROR("[Fail] Dispatch incorrect. Sum = {0}", dispatchSum.load());
        }

        // C. Test Reactive Wakeup from Idle Park (B24 verification & Review item A3)
        BB_CORE_INFO("Testing reactive wakeup from idle park (B24 & Review item A3)...");
        // Let workers settle into deep park (idle state)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        // Specifically test that a sleeping worker wakes up WITHOUT caller assist (no jobSystem.wait)
        std::atomic<bool> workerOnlyExecuted{false};
        auto tStart = std::chrono::high_resolution_clock::now();

        jobSystem.execute([&workerOnlyExecuted]() {
            workerOnlyExecuted.store(true, std::memory_order_release);
        });

        // Main thread waits passively on the atomic flag (cannot assist by popping)
        int waitAttempts = 0;
        while (!workerOnlyExecuted.load(std::memory_order_acquire) && waitAttempts < 500) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            waitAttempts++;
        }
        auto tEnd = std::chrono::high_resolution_clock::now();
        auto wakeDurationUs = std::chrono::duration_cast<std::chrono::microseconds>(tEnd - tStart).count();

        if (workerOnlyExecuted.load() && wakeDurationUs < 50000) {
            BB_CORE_INFO("[Success] Pure worker wakeup from idle park completed in {} us (without caller assist).", wakeDurationUs);
        } else {
            BB_CORE_ERROR("[Fail] Worker wakeup failed or timed out (duration: {} us).", wakeDurationUs);
            throw std::runtime_error("Worker wakeup test failed");
        }

        // D. Test Exception Safe
        jobSystem.executeSafe([]() {
            BB_CORE_WARN("Job: Deliberate test exception (expected behavior).");
            throw std::runtime_error("Voluntary error to test executeSafe logging");
        });

        // E. Test Stop Token (Long Running Job)
        std::atomic<bool> longJobStarted{false};
        std::atomic<bool> longJobStopped{false};

        jobSystem.execute([&](std::stop_token st) {
            longJobStarted = true;
            BB_CORE_INFO("LongJob: Started.");
            while (!st.stop_requested()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            longJobStopped = true;
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        BB_CORE_INFO("Shutting down JobSystem...");
        jobSystem.shutdown();

        if (longJobStopped) {
            BB_CORE_INFO("[Success] LongJob interrupted cleanly.");
        } else {
            BB_CORE_WARN("[Warn] LongJob status uncertain (Start:{}, Stop:{}).", longJobStarted.load(), longJobStopped.load());
        }
    }

    // 2. TEST EVENT BUS
    {
        BB_CORE_INFO("[Test] EventBus...");
        bb3d::EventBus eventBus;
        bool received = false;
        int receivedId = 0;

        eventBus.subscribe<TestEvent>([&](const TestEvent& e) {
            BB_CORE_INFO("Event Received : [{}] {}", e.id, e.message);
            received = true;
            receivedId = e.id;
        });

        TestEvent evt{42, "Hello EventBus"};
        eventBus.publish(evt);

        if (received && receivedId == 42) {
            BB_CORE_INFO("[Success] EventBus correctly dispatched event.");
        } else {
            BB_CORE_ERROR("[Fail] EventBus did not receive event.");
            throw std::runtime_error("EventBus test failed");
        }

        // Test Multi-Subscribe
        bool p1 = false, p2 = false;
        eventBus.subscribe<PlayerDiedEvent>([&](const PlayerDiedEvent&){ p1 = true; });
        eventBus.subscribe<PlayerDiedEvent>([&](const PlayerDiedEvent&){ p2 = true; });
        
        eventBus.publish(PlayerDiedEvent{1});
        
        if (p1 && p2) BB_CORE_INFO("[Success] EventBus : Multi-subscriber OK.");
        else BB_CORE_ERROR("[Fail] EventBus : Multi-subscriber failed.");

        // Test Queue (Deferred)
        bool queuedReceived = false;
        eventBus.subscribe<std::string>([&](const std::string& msg) {
            BB_CORE_INFO("Deferred Event Received : {}", msg);
            queuedReceived = true;
        });

        eventBus.enqueue(std::string("Deferred message"));
        
        if (queuedReceived) {
            BB_CORE_ERROR("[Fail] Deferred event was processed too early!");
        }

        BB_CORE_INFO("Dispatching queue...");
        eventBus.dispatchQueued();

        if (queuedReceived) {
            BB_CORE_INFO("[Success] EventBus : Queue dispatch OK.");
        } else {
            BB_CORE_ERROR("[Fail] EventBus : Queue dispatch failed.");
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