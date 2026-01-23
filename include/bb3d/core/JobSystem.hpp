#pragma once

#include "bb3d/core/Log.hpp"

#include <functional>
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <stop_token>
#include <concepts>

namespace bb3d {

class JobSystem {
public:
    JobSystem();
    ~JobSystem();

    // Initialise le pool de threads
    void init(uint32_t threadCount = 0);
    
    // Arrête les threads proprement (signale le stop_token)
    void shutdown();

    // Retourne le nombre de threads travailleurs
    uint32_t getThreadCount() const { return static_cast<uint32_t>(m_workers.size()); }

    // --- API Fluent & Templates ---

    /**
     * @brief Ajoute une tâche à la file d'attente.
     * Supporte les lambdas void() et void(std::stop_token).
     */
    template<typename Callable>
    void execute(Callable&& job) {
        // Wrapper pour normaliser la signature en void(std::stop_token)
        if constexpr (std::invocable<Callable, std::stop_token>) {
            pushInternal(std::forward<Callable>(job));
        } else {
            // Si le job ne prend pas de token, on l'emballe
            pushInternal([job = std::forward<Callable>(job)](std::stop_token) {
                job();
            });
        }
    }

    /**
     * @brief Exécute une tâche dans un bloc try-catch pour éviter de crasher le worker.
     * Idéal pour les tâches risquées (IO, parsing).
     */
    template<typename Callable>
    void executeSafe(Callable&& job) {
        execute([job = std::forward<Callable>(job)](std::stop_token st) {
            try {
                if constexpr (std::invocable<Callable, std::stop_token>) {
                    job(st);
                } else {
                    job();
                }
            } catch (const std::exception& e) {
                BB_CORE_ERROR("JobSystem: Exception caught in job: {}", e.what());
            } catch (...) {
                BB_CORE_ERROR("JobSystem: Unknown exception caught in job.");
            }
        });
    }

private:
    void workerLoop(std::stop_token st);
    void pushInternal(std::function<void(std::stop_token)>&& job);

    std::vector<std::jthread> m_workers; // C++20: gère join/stop auto
    std::queue<std::function<void(std::stop_token)>> m_jobQueue;
    
    std::mutex m_queueMutex;
    std::condition_variable_any m_condition; // C++20: compatible stop_token
};

} // namespace bb3d
