#include "bb3d/core/JobSystem.hpp"
#include "bb3d/core/Log.hpp"

#if defined(_MSC_VER) || defined(__i386__) || defined(__x86_64__)
#include <emmintrin.h>
#endif

namespace bb3d {

JobSystem::JobSystem() {}

JobSystem::~JobSystem() {
    shutdown();
}

void JobSystem::init(uint32_t threadCount) {
    if (!m_workers.empty()) return;

    if (threadCount == 0) {
        threadCount = std::max(1u, std::thread::hardware_concurrency() - 1);
    }

    BB_CORE_INFO("JobSystem: Initialized with {0} worker threads (Work Stealing).", threadCount);

    // Initialize worker queues (1:1 mapping with worker threads to simplify distribution)
    m_queues.reserve(threadCount);
    for (uint32_t i = 0; i < threadCount; ++i) {
        m_queues.push_back(std::make_unique<WorkerQueue>());
    }

    m_workers.reserve(threadCount);
    for (uint32_t i = 0; i < threadCount; ++i) {
        m_workers.emplace_back(std::bind_front(&JobSystem::workerLoop, this, i));
    }
}

void JobSystem::shutdown() {
    if (m_workers.empty()) return;

    BB_CORE_INFO("JobSystem: Shutting down {0} workers...", m_workers.size());
    for (auto& worker : m_workers) {
        worker.request_stop();
    }
    
    m_globalCondition.notify_all();
    m_workers.clear();
    m_queues.clear();
    m_pendingJobs.store(0, std::memory_order_relaxed);
    m_sleepingWorkers.store(0, std::memory_order_relaxed);
}

void JobSystem::pushInternal(std::function<void(std::stop_token)>&& job) {
    const uint32_t queueCount = static_cast<uint32_t>(m_queues.size());
    if (queueCount == 0) return;

    // Round-Robin dispatch: distribute jobs evenly across worker queues
    const uint32_t queueIndex = m_nextQueueIndex.fetch_add(1, std::memory_order_relaxed) % queueCount;

    {
        std::lock_guard<std::mutex> lock(m_queues[queueIndex]->mutex);
        m_queues[queueIndex]->queue.push_back({std::move(job)});
    }
    
    m_pendingJobs.fetch_add(1, std::memory_order_release);

    // Fast-path: only wake up a worker if at least one is currently sleeping.
    // If all workers are busy running tasks, notify is skipped entirely (zero overhead).
    if (m_sleepingWorkers.load(std::memory_order_relaxed) > 0) {
        m_globalCondition.notify_one();
    }
}

bool JobSystem::popJob(Job& outJob, uint32_t threadIndex) {
    const uint32_t queueCount = static_cast<uint32_t>(m_queues.size());
    if (queueCount == 0) return false;

    // 1. Try local queue first (Fast Path)
    uint32_t localIdx = threadIndex % queueCount;
    {
        std::unique_lock<std::mutex> lock(m_queues[localIdx]->mutex, std::try_to_lock);
        if (lock.owns_lock() && !m_queues[localIdx]->queue.empty()) {
            outJob = std::move(m_queues[localIdx]->queue.front());
            m_queues[localIdx]->queue.pop_front();
            m_pendingJobs.fetch_sub(1, std::memory_order_release);
            return true;
        }
    }

    // 2. Randomized work stealing from other queues
    // Start at an offset to avoid every thread contending for queue 0
    static thread_local uint32_t stealOffset = threadIndex;
    stealOffset++; 

    for (uint32_t i = 0; i < queueCount; ++i) {
        uint32_t targetIdx = (stealOffset + i) % queueCount;
        if (targetIdx == localIdx) continue;

        std::unique_lock<std::mutex> lock(m_queues[targetIdx]->mutex, std::try_to_lock);
        if (lock.owns_lock() && !m_queues[targetIdx]->queue.empty()) {
            outJob = std::move(m_queues[targetIdx]->queue.front());
            m_queues[targetIdx]->queue.pop_front();
            m_pendingJobs.fetch_sub(1, std::memory_order_release);
            return true;
        }
    }

    return false;
}

void JobSystem::workerLoop(uint32_t threadIndex, std::stop_token st) {
    uint32_t spinCount = 0;
    constexpr uint32_t MAX_SPIN = 64;

    while (!st.stop_requested()) {
        Job job;
        if (popJob(job, threadIndex)) {
            job.task(st);
            spinCount = 0; // Reset spin counter on successful task execution
        } else if (spinCount < MAX_SPIN) {
            // Stage 1: Ultra-fast hardware pause (~15ns) to stay hot during active frame tasks
#if defined(_MSC_VER)
            _mm_pause();
#elif defined(__i386__) || defined(__x86_64__)
            __builtin_ia32_pause();
#endif
            spinCount++;
        } else {
            // Stage 2: Genuine idle state (end of frame / engine paused) -> deep park in OS
            std::unique_lock<std::mutex> lock(m_globalMutex);
            m_sleepingWorkers.fetch_add(1, std::memory_order_relaxed);
            m_globalCondition.wait(lock, st, [&]() {
                return st.stop_requested() || m_pendingJobs.load(std::memory_order_acquire) > 0;
            });
            m_sleepingWorkers.fetch_sub(1, std::memory_order_relaxed);
            spinCount = 0;
        }
    }
}

void JobSystem::dispatch(uint32_t jobCount, uint32_t groupSize, const std::function<void(uint32_t, uint32_t)>& func) {
    if (jobCount == 0 || groupSize == 0) return;

    // Calculate number of batches
    const uint32_t groupCount = (jobCount + groupSize - 1) / groupSize;
    
    // Shared completion counter
    auto counter = std::make_shared<std::atomic<int>>(groupCount);

    for (uint32_t i = 0; i < groupCount; ++i) {
        // Capture parameters for each batch
        execute([func, i, groupSize, jobCount](std::stop_token) {
            uint32_t start = i * groupSize;
            uint32_t end = std::min(start + groupSize, jobCount);
            for (uint32_t j = start; j < end; ++j) {
                func(j, 1); 
            }
        }, counter);
    }

    // Synchronously wait for all batches to complete
    wait(counter);
}

void JobSystem::wait(const JobCounter& counter) {
    if (!counter) return;

    uint32_t yieldCount = 0;
    while (counter->load(std::memory_order_acquire) > 0) {
        Job job;
        // The calling thread assists by executing pending jobs
        if (popJob(job, m_callerIndex.fetch_add(1, std::memory_order_relaxed))) {
            job.task(std::stop_token{}); 
            yieldCount = 0;
        } else {
            // Yield or pause if no job is immediately available
            if (yieldCount++ < 100) {
#if defined(_MSC_VER)
                _mm_pause();
#elif defined(__i386__) || defined(__x86_64__)
                __builtin_ia32_pause();
#endif
            } else {
                std::this_thread::yield();
                yieldCount = 0;
            }
        }
    }
}

} // namespace bb3d
