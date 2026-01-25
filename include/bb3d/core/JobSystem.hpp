#pragma once

#include "bb3d/core/Base.hpp"
#include "bb3d/core/Log.hpp"
#include <functional>
#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <stop_token>
#include <future>
#include <concepts>

namespace bb3d {

/**
 * @brief Un compteur atomique pour suivre l'achèvement d'un groupe de tâches.
 */
using JobCounter = std::shared_ptr<std::atomic<int>>;

/**
 * @brief Système de gestion de tâches (Job System) multi-threadé.
 * 
 * Utilise un Thread Pool avec vol de travail (Work Stealing) pour répartir
 * la charge CPU sur tous les cœurs disponibles.
 */
class JobSystem {
public:
    JobSystem();
    ~JobSystem();

    /**
     * @brief Initialise le pool de threads.
     * @param threadCount Nombre de threads (0 pour détection automatique).
     */
    void init(uint32_t threadCount = 0);
    
    /**
     * @brief Arrête proprement tous les threads.
     */
    void shutdown();

    /** @brief Récupère le nombre de threads actifs. */
    [[nodiscard]] inline uint32_t getThreadCount() const { return static_cast<uint32_t>(m_workers.size()); }

    /**
     * @brief Lance un job asynchrone.
     * @param job La fonction à exécuter (Callable).
     * @param counter (Optionnel) Un compteur à décrémenter à la fin.
     */
    template<typename Callable>
    void execute(Callable&& job, JobCounter counter = nullptr) {
        auto wrappedJob = [job = std::forward<Callable>(job), counter](std::stop_token st) mutable {
            if constexpr (std::invocable<Callable, std::stop_token>) {
                job(st);
            } else {
                job();
            }
            if (counter) {
                counter->fetch_sub(1, std::memory_order_release);
                counter->notify_all(); 
            }
        };

        pushInternal(std::move(wrappedJob));
    }

    /**
     * @brief Exécute un job de manière sécurisée (capture les exceptions).
     */
    template<typename Callable>
    void executeSafe(Callable&& job, JobCounter counter = nullptr) {
        execute([job = std::forward<Callable>(job)](std::stop_token st) {
            try {
                if constexpr (std::invocable<Callable, std::stop_token>) {
                    job(st);
                } else {
                    job();
                }
            } catch (const std::exception& e) {
                BB_CORE_ERROR("JobSystem: Exception caught: {0}", e.what());
            } catch (...) {
                BB_CORE_ERROR("JobSystem: Unknown exception caught.");
            }
        }, counter);
    }

    /**
     * @brief Découpe une boucle en plusieurs jobs parallèles (Parallel For).
     * @param jobCount Nombre total d'itérations.
     * @param groupSize Taille du batch par thread.
     * @param func Fonction prenant (jobIndex, count).
     */
    void dispatch(uint32_t jobCount, uint32_t groupSize, const std::function<void(uint32_t, uint32_t)>& func);

    /**
     * @brief Attend qu'un compteur atteigne 0.
     * @note Le thread appelant aide à exécuter les jobs en attente durant le wait.
     */
    void wait(const JobCounter& counter);

private:
    struct Job {
        std::function<void(std::stop_token)> task;
    };

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324) 
#endif
    struct alignas(64) WorkerQueue {
        std::mutex mutex;
        std::deque<Job> queue;
    };
#ifdef _MSC_VER
#pragma warning(pop)
#endif

    void workerLoop(uint32_t threadIndex, std::stop_token st);
    void pushInternal(std::function<void(std::stop_token)>&& job);
    bool popJob(Job& outJob, uint32_t threadIndex);

    std::vector<std::jthread> m_workers;
    std::vector<std::unique_ptr<WorkerQueue>> m_queues; 
    std::atomic<uint32_t> m_nextQueueIndex{0}; 
    
    std::condition_variable_any m_globalCondition;
    std::mutex m_globalMutex; 
};

} // namespace bb3d