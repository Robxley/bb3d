#pragma once

#include "bb3d/core/Base.hpp"
#include "bb3d/core/Log.hpp" // Added
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

// Un compteur atomique pour suivre la fin d'un groupe de tâches
using JobCounter = std::shared_ptr<std::atomic<int>>;

class JobSystem {
public:
    JobSystem();
    ~JobSystem();

    void init(uint32_t threadCount = 0);
    void shutdown();

    uint32_t getThreadCount() const { return static_cast<uint32_t>(m_workers.size()); }

    // --- API Fluent & Templates ---

    /**
     * @brief Lance un job asynchrone.
     * @param job La fonction à exécuter.
     * @param counter (Optionnel) Un compteur à décrémenter à la fin du job.
     */
    template<typename Callable>
    void execute(Callable&& job, JobCounter counter = nullptr) {
        // Wrapper normalisé
        auto wrappedJob = [job = std::forward<Callable>(job), counter](std::stop_token st) mutable {
            if constexpr (std::invocable<Callable, std::stop_token>) {
                job(st);
            } else {
                job();
            }
            if (counter) {
                counter->fetch_sub(1, std::memory_order_release);
                // Notification globale (pour réveiller ceux qui attendent sur le counter)
                // Note: std::atomic::notify_all est C++20
                counter->notify_all(); 
            }
        };

        pushInternal(std::move(wrappedJob));
    }

    /**
     * @brief Exécute safe (try-catch) avec support compteur.
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
                BB_CORE_ERROR("JobSystem: Exception caught in job: {0}", e.what());
            } catch (...) {
                BB_CORE_ERROR("JobSystem: Unknown exception caught in job.");
            }
        }, counter);
    }

    /**
     * @brief Découpe une boucle en plusieurs jobs parallèles.
     * @param jobCount Nombre total d'éléments.
     * @param groupSize Nombre d'éléments par job (Batching).
     * @param func Fonction prenant (jobIndex, count).
     */
    void dispatch(uint32_t jobCount, uint32_t groupSize, const std::function<void(uint32_t, uint32_t)>& func);

    /**
     * @brief Attend qu'un compteur atteigne 0.
     * IMPORTANT : En attendant, le thread appelant AIDE à exécuter des jobs !
     */
    void wait(const JobCounter& counter);

private:
    struct Job {
        std::function<void(std::stop_token)> task;
    };

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324) // Structure was padded due to alignment specifier
#endif
    // Structure alignée pour éviter le "False Sharing" entre threads
    struct alignas(64) WorkerQueue {
        std::mutex mutex;
        std::deque<Job> queue;
    };
#ifdef _MSC_VER
#pragma warning(pop)
#endif

    void workerLoop(uint32_t threadIndex, std::stop_token st);
    void pushInternal(std::function<void(std::stop_token)>&& job);
    
    // Tente de récupérer un job (Local -> Vol -> Rien)
    bool popJob(Job& outJob, uint32_t threadIndex);

    std::vector<std::jthread> m_workers;
    std::vector<std::unique_ptr<WorkerQueue>> m_queues; // Pointeurs pour garantir l'adresse stable
    
    std::atomic<uint32_t> m_nextQueueIndex{0}; // Pour le Round-Robin dispatch
    
    // Condition globale pour réveiller les workers qui dorment
    std::condition_variable_any m_globalCondition;
    std::mutex m_globalMutex; // Utilisé uniquement pour le wait de la CV
};

} // namespace bb3d
