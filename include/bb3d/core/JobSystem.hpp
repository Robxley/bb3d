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
 * @brief Atomic counter tracking completion of a group of tasks.
 */
using JobCounter = std::shared_ptr<std::atomic<int>>;

/**
 * @brief Multi-threaded task scheduling system (Job System).
 * 
 * Uses a work-stealing thread pool to balance CPU load across all available cores.
 */
class JobSystem {
public:
    JobSystem();
    ~JobSystem();

    /**
     * @brief Initializes the thread pool.
     * @param threadCount Number of worker threads (0 for automatic core count detection).
     */
    void init(uint32_t threadCount = 0);
    
    /**
     * @brief Stops all worker threads cleanly.
     */
    void shutdown();

    /** @brief Returns the number of active worker threads. */
    [[nodiscard]] inline uint32_t getThreadCount() const { return static_cast<uint32_t>(m_workers.size()); }

    /**
     * @brief Submits an asynchronous job.
     * @param job Callable task to execute.
     * @param counter (Optional) Shared counter decremented upon completion.
     */
    template<typename Callable>
    void execute(Callable&& job, JobCounter counter = nullptr) {
        auto wrappedJob = [job = std::forward<Callable>(job), counter]([[maybe_unused]] std::stop_token st) mutable {
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
     * @brief Executes a job with exception safety (catches and logs exceptions).
     */
    template<typename Callable>
    void executeSafe(Callable&& job, JobCounter counter = nullptr) {
        execute([job = std::forward<Callable>(job)]([[maybe_unused]] std::stop_token st) mutable {
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
     * @brief Splits a loop into multiple parallel jobs (Parallel For).
     * @param jobCount Total number of iterations.
     * @param groupSize Batch size per thread.
     * @param func Function accepting (jobIndex, count).
     */
    void dispatch(uint32_t jobCount, uint32_t groupSize, const std::function<void(uint32_t, uint32_t)>& func);

    /**
     * @brief Waits until the job counter reaches 0.
     * @note The calling thread assists by executing pending jobs while waiting.
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

    // Hybrid reactive wake & idle management
    std::atomic<int32_t> m_pendingJobs{0};
    std::atomic<uint32_t> m_sleepingWorkers{0};
    std::atomic<uint32_t> m_callerIndex{0};
};

} // namespace bb3d