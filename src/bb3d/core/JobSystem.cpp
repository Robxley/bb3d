#include "bb3d/core/JobSystem.hpp"
#include "bb3d/core/Log.hpp"

namespace bb3d {

JobSystem::JobSystem() {}

JobSystem::~JobSystem() {
    shutdown();
}

void JobSystem::init(uint32_t threadCount) {
    if (!m_workers.empty()) return; // Déjà init

    if (threadCount == 0) {
        threadCount = std::max(1u, std::thread::hardware_concurrency() - 1);
    }

    BB_CORE_INFO("JobSystem: Initialisation avec {} threads (jthread).", threadCount);

    m_workers.reserve(threadCount);
    for (uint32_t i = 0; i < threadCount; ++i) {
        // jthread passe automatiquement son stop_token à la fonction
        m_workers.emplace_back(std::bind_front(&JobSystem::workerLoop, this));
    }
}

void JobSystem::shutdown() {
    // Avec std::jthread, le destructeur appelle request_stop() et join().
    // Mais on peut vouloir forcer l'arrêt manuellement.
    for (auto& worker : m_workers) {
        worker.request_stop();
    }
    
    // On réveille tout le monde pour qu'ils voient le stop_token
    m_condition.notify_all();
    
    // Le clear va détruire les jthreads, donc faire le join()
    m_workers.clear();
}

void JobSystem::pushInternal(std::function<void(std::stop_token)>&& job) {
    {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        m_jobQueue.push(std::move(job));
    }
    m_condition.notify_one();
}

void JobSystem::workerLoop(std::stop_token st) {
    while (!st.stop_requested()) {
        std::function<void(std::stop_token)> job;

        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            
            // C++20 Magic : wait prend le stop_token !
            // La fonction se réveille si :
            // 1. notify est appelé ET le prédicat est vrai.
            // 2. st.stop_requested() devient vrai.
            bool available = m_condition.wait(lock, st, [this] {
                return !m_jobQueue.empty();
            });

            if (!available) {
                // Réveil dû au stop_token
                return;
            }

            job = std::move(m_jobQueue.front());
            m_jobQueue.pop();
        }

        // Exécution hors du lock avec passage du token
        if (job) {
            job(st);
        }
    }
}

} // namespace bb3d