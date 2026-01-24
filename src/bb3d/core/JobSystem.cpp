#include "bb3d/core/JobSystem.hpp"
#include "bb3d/core/Log.hpp"

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

    BB_CORE_INFO("JobSystem: Initialisation avec {0} threads (Work Stealing).", threadCount);

    // Initialisation des queues (une par thread + 1 pour le main thread si besoin)
    // On crée autant de queues que de workers pour simplifier le mapping 1:1
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
}

void JobSystem::pushInternal(std::function<void(std::stop_token)>&& job) {
    // Round-Robin Dispatch : On distribue équitablement les tâches
    // Cela évite qu'une seule queue soit surchargée (diminue la nécessité du vol)
    const uint32_t queueCount = static_cast<uint32_t>(m_queues.size());
    const uint32_t queueIndex = m_nextQueueIndex.fetch_add(1, std::memory_order_relaxed) % queueCount;

    {
        std::lock_guard<std::mutex> lock(m_queues[queueIndex]->mutex);
        m_queues[queueIndex]->queue.push_back({std::move(job)});
    }
    
    // Réveil massif pour garantir que les jobs sont pris en charge rapidement
    m_globalCondition.notify_all();
}

bool JobSystem::popJob(Job& outJob, uint32_t threadIndex) {
    const uint32_t queueCount = static_cast<uint32_t>(m_queues.size());
    if (queueCount == 0) return false;

    // 1. Essayer la queue locale (Fast Path)
    uint32_t localIdx = threadIndex % queueCount;
    {
        std::unique_lock<std::mutex> lock(m_queues[localIdx]->mutex, std::try_to_lock);
        if (lock.owns_lock() && !m_queues[localIdx]->queue.empty()) {
            outJob = std::move(m_queues[localIdx]->queue.front());
            m_queues[localIdx]->queue.pop_front();
            return true;
        }
    }

    // 2. Work Stealing Randomisé
    // On commence à un endroit aléatoire pour éviter que tous les threads ne se battent pour la queue 0
    static thread_local uint32_t stealOffset = threadIndex;
    stealOffset++; 

    for (uint32_t i = 0; i < queueCount; ++i) {
        uint32_t targetIdx = (stealOffset + i) % queueCount;
        if (targetIdx == localIdx) continue;

        std::unique_lock<std::mutex> lock(m_queues[targetIdx]->mutex, std::try_to_lock);
        if (lock.owns_lock() && !m_queues[targetIdx]->queue.empty()) {
            outJob = std::move(m_queues[targetIdx]->queue.front());
            m_queues[targetIdx]->queue.pop_front();
            return true;
        }
    }

    return false;
}

void JobSystem::workerLoop(uint32_t threadIndex, std::stop_token st) {
    while (!st.stop_requested()) {
        Job job;
        if (popJob(job, threadIndex)) {
            job.task(st);
        } else {
            // Pas de travail trouvé : Attente passive courte
            std::unique_lock<std::mutex> lock(m_globalMutex);
            // C++20 Standard : wait_for(lock, stop_token, timeout, predicate)
            m_globalCondition.wait_for(lock, st, std::chrono::milliseconds(1), [&]() {
                 return false; 
            });
        }
    }
}

void JobSystem::dispatch(uint32_t jobCount, uint32_t groupSize, const std::function<void(uint32_t, uint32_t)>& func) {
    if (jobCount == 0 || groupSize == 0) return;

    // Calcul du nombre de groupes
    const uint32_t groupCount = (jobCount + groupSize - 1) / groupSize;
    
    // Compteur partagé
    auto counter = std::make_shared<std::atomic<int>>(groupCount);

    for (uint32_t i = 0; i < groupCount; ++i) {
        // Capture des paramètres pour chaque job
        execute([func, i, groupSize, jobCount](std::stop_token) {
            uint32_t start = i * groupSize;
            uint32_t end = std::min(start + groupSize, jobCount);
            // Appel de la fonction utilisateur avec [index, count] pour le batch
            for (uint32_t j = start; j < end; ++j) {
                func(j, 1); 
            }
        }, counter);
    }

    // Optionnel : On peut attendre ici, ou laisser l'utilisateur attendre.
    // Pour être cohérent avec "dispatch" (synchrone du point de vue appelant), on attend.
    wait(counter);
}

void JobSystem::wait(const JobCounter& counter) {
    if (!counter) return;

    uint32_t yieldCount = 0;
    // Tant que le compteur n'est pas à 0
    while (counter->load(std::memory_order_acquire) > 0) {
        Job job;
        // Le Main Thread (ou appelant) participe !
        // On utilise un index "fictif" (ex: tournant) pour qu'il vole un peu partout
        static std::atomic<uint32_t> callerIndex{0};
        
        if (popJob(job, callerIndex++)) {
            job.task(std::stop_token{}); 
            yieldCount = 0;
        } else {
            // Si vraiment rien à faire, on laisse la main
            if (yieldCount++ < 100) {
                // Pause CPU ultra-courte (hint) pour éviter de chauffer
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
