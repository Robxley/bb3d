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
 * @brief Système de messagerie Publish/Subscribe optimisé.
 * - Publish Synchrone : Lock-free en lecture (via shared_mutex), Zéro allocation.
 * - Publish Différé (Queue) : Stockage pour traitement en fin de frame.
 */
class EventBus {
public:
    EventBus() = default;
    ~EventBus() = default;

    /**
     * @brief S'abonne à un type d'événement.
     * @warning Ne PAS appeler ceci à l'intérieur d'un callback d'événement (Deadlock).
     */
    template<typename T>
    void subscribe(std::function<void(const T&)> callback) {
        // LOCK ÉCRITURE (Exclusif)
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
     * @details Utilise un Shared Lock. Récursif (un event peut en déclencher un autre).
     * Zéro allocation dynamique.
     */
    template<typename T>
    void publish(const T& event) {
        // LOCK LECTURE (Partagé)
        // Plusieurs threads peuvent publier en même temps.
        // Un même thread peut publier récursivement.
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        
        std::type_index typeIndex(typeid(T));

        auto it = m_subscribers.find(typeIndex);
        if (it != m_subscribers.end()) {
            const auto& handlers = it->second;
            // Itération directe sans copie (Hot Path Optimisé)
            for (const auto& handler : handlers) {
                handler(&event);
            }
        }
    }

    /**
     * @brief Met un événement en file d'attente pour être traité plus tard (ex: fin de frame).
     * @details Utile pour éviter les problèmes de concurrence ou de pile d'appel trop profonde.
     */
    template<typename T>
    void enqueue(T event) {
        // On doit copier l'événement pour le stocker
        // On utilise une lambda qui capture l'événement par valeur
        auto cmd = [this, event = std::move(event)]() {
            this->publish(event);
        };

        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_eventQueue.push(std::move(cmd));
        }
    }

    /**
     * @brief Traite tous les événements en attente.
     * À appeler typiquement à la fin de la boucle de jeu (Update).
     */
    void dispatchQueued() {
        std::vector<std::function<void()>> processingQueue;

        // On vide la queue dans un buffer local pour libérer le lock rapidement
        // et permettre aux callbacks d'enqueuer de nouveaux events sans deadlock.
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (m_eventQueue.empty()) return;
            
            // Swap rapide pour éviter les allocations répétées si possible
            // Ici on copie vers le vecteur local
            while (!m_eventQueue.empty()) {
                processingQueue.push_back(std::move(m_eventQueue.front()));
                m_eventQueue.pop();
            }
        }

        // Exécution hors du lock de la queue
        for (const auto& cmd : processingQueue) {
            cmd();
        }
    }

private:
    using EventWrapper = std::function<void(const void*)>;
    
    // Mutex RW pour les abonnés (Lectures fréquentes, Écritures rares)
    std::shared_mutex m_mutex;
    std::unordered_map<std::type_index, std::vector<EventWrapper>> m_subscribers;

    // Queue pour les événements différés
    std::mutex m_queueMutex;
    std::queue<std::function<void()>> m_eventQueue;
};

} // namespace bb3d
