#pragma once

#include <functional>
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <any>
#include <mutex>

namespace bb3d {

/**
 * @brief Système de messagerie Publish/Subscribe typé.
 * Permet de découpler les systèmes (ex: Input -> Gameplay -> Audio).
 */
class EventBus {
public:
    using HandlerID = uint32_t;

    EventBus() = default;
    ~EventBus() = default;

    /**
     * @brief S'abonne à un type d'événement spécifique.
     * @tparam T Le type de la structure d'événement.
     * @param callback La fonction à appeler.
     */
    template<typename T>
    void subscribe(std::function<void(const T&)> callback) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::type_index typeIndex(typeid(T));

        // Wrapper générique pour stocker la callback typée dans une liste générique
        auto wrapper = [callback](const void* eventData) {
            const T* event = static_cast<const T*>(eventData);
            callback(*event);
        };

        m_subscribers[typeIndex].push_back(wrapper);
    }

    /**
     * @brief Publie un événement immédiatement.
     * @tparam T Le type de l'événement.
     * @param event L'instance de l'événement.
     */
    template<typename T>
    void publish(const T& event) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::type_index typeIndex(typeid(T));

        if (m_subscribers.find(typeIndex) != m_subscribers.end()) {
            const auto& handlers = m_subscribers[typeIndex];
            for (const auto& handler : handlers) {
                handler(&event);
            }
        }
    }

private:
    using EventWrapper = std::function<void(const void*)>;
    
    std::mutex m_mutex;
    std::unordered_map<std::type_index, std::vector<EventWrapper>> m_subscribers;
};

} // namespace bb3d
