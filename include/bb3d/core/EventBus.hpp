#pragma once

#include "bb3d/core/Base.hpp"
#include <functional>
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <shared_mutex>
#include <mutex>
#include <queue>

namespace bb3d {

/**
 * @brief Système de messagerie Publish/Subscribe découplé.
 * 
 * Permet la communication entre systèmes sans dépendances directes.
 * Supporte la publication synchrone et différée.
 */
class EventBus {
public:
    EventBus() = default;
    ~EventBus() = default;

    /**
     * @brief S'abonne à un type d'événement.
     * @tparam T Type de l'événement.
     * @param callback Fonction de rappel.
     */
    template<typename T>
    void subscribe(std::function<void(const T&)> callback) {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        std::type_index typeIndex(typeid(T));

        auto wrapper = [callback](const void* eventData) {
            const T* event = static_cast<const T*>(eventData);
            callback(*event);
        };

        m_subscribers[typeIndex].push_back(wrapper);
    }

    /**
     * @brief Publie un événement immédiatement (Synchrone).
     * @tparam T Type de l'événement.
     * @param event Données de l'événement.
     */
    template<typename T>
    void publish(const T& event) {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        std::type_index typeIndex(typeid(T));

        auto it = m_subscribers.find(typeIndex);
        if (it != m_subscribers.end()) {
            for (const auto& handler : it->second) {
                handler(&event);
            }
        }
    }

    /**
     * @brief Met un événement en file d'attente (Asynchrone).
     * @tparam T Type de l'événement.
     * @param event Données de l'événement.
     */
    template<typename T>
    void enqueue(T event) {
        auto cmd = [this, event = std::move(event)]() {
            this->publish(event);
        };

        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_eventQueue.push(std::move(cmd));
        }
    }

    /**
     * @brief Traite tous les événements mis en file d'attente.
     * Doit être appelé à la fin de chaque frame.
     */
    void dispatchQueued() {
        std::vector<std::function<void()>> processingQueue;

        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (m_eventQueue.empty()) return;
            
            while (!m_eventQueue.empty()) {
                processingQueue.push_back(std::move(m_eventQueue.front()));
                m_eventQueue.pop();
            }
        }

        for (const auto& cmd : processingQueue) {
            cmd();
        }
    }

private:
    using EventWrapper = std::function<void(const void*)>;
    
    std::shared_mutex m_mutex;
    std::unordered_map<std::type_index, std::vector<EventWrapper>> m_subscribers;

    std::mutex m_queueMutex;
    std::queue<std::function<void()>> m_eventQueue;
};

} // namespace bb3d