#pragma once

#include "bb3d/core/Base.hpp"
#include "bb3d/resource/Resource.hpp"
#include "bb3d/core/JobSystem.hpp"
#include "bb3d/core/Log.hpp"

#include <unordered_map>
#include <string>
#include <string_view>
#include <shared_mutex>
#include <functional>
#include <typeindex>
#include <type_traits>

namespace bb3d {

/** @brief Interface interne pour l'effacement générique du cache. */
class IResourceCache {
public:
    virtual ~IResourceCache() = default;
    virtual void clear() = 0;
};

/** @brief Foncteur de hachage transparent pour optimiser les recherches par string_view. */
struct StringHash {
    using is_transparent = void;
    size_t operator()(std::string_view sv) const { return std::hash<std::string_view>{}(sv); }
    size_t operator()(const std::string& s) const { return std::hash<std::string>{}(s); }
    size_t operator()(const char* s) const { return std::hash<std::string_view>{}(s); }
};

class VulkanContext;
class ResourceManager;

/**
 * @brief Cache spécialisé pour un type de ressource donné.
 * @tparam T Type de la ressource (doit dériver de Resource).
 */
template<typename T>
class ResourceCache : public IResourceCache {
public:
    ResourceCache(VulkanContext& context, ResourceManager* manager) 
        : m_context(context), m_manager(manager) {}

    /**
     * @brief Récupère une ressource du cache ou la charge si nécessaire.
     */
    template<typename... Args>
    Ref<T> getOrLoad(std::string_view path, Args&&... args) {
        {
            std::shared_lock<std::shared_mutex> readLock(m_mutex);
            auto it = m_resources.find(path);
            if (it != m_resources.end()) return it->second;
        }

        std::unique_lock<std::shared_mutex> writeLock(m_mutex);
        auto it = m_resources.find(path);
        if (it != m_resources.end()) return it->second;

        BB_CORE_INFO("ResourceCache: Loading '{0}'", path);
        
        Ref<T> resource = nullptr;
        try {
            if constexpr (std::is_constructible_v<T, VulkanContext&, ResourceManager&, std::string, Args...>) {
                resource = CreateRef<T>(m_context, *m_manager, std::string(path), std::forward<Args>(args)...);
            } else if constexpr (std::is_constructible_v<T, VulkanContext&, std::string, Args...>) {
                resource = CreateRef<T>(m_context, std::string(path), std::forward<Args>(args)...);
            }
        } catch (const std::exception& e) {
            BB_CORE_ERROR("ResourceCache: Failed to load '{0}': {1}", path, e.what());
            return nullptr; 
        }

        m_resources[std::string(path)] = resource;
        return resource;
    }

    void clear() override {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_resources.clear();
    }

private:
    VulkanContext& m_context;
    ResourceManager* m_manager;
    std::shared_mutex m_mutex;
    std::unordered_map<std::string, Ref<T>, StringHash, std::equal_to<>> m_resources;
};

/**
 * @brief Gestionnaire global des ressources avec mise en cache et chargement asynchrone.
 */
class ResourceManager {
public:
    ResourceManager(VulkanContext& context, JobSystem& jobSystem);
    ~ResourceManager() = default;

    /**
     * @brief Charge une ressource de manière synchrone.
     * @tparam T Type de la ressource.
     */
    template<typename T, typename... Args>
    Ref<T> load(std::string_view path, Args&&... args) {
        return getCache<T>().getOrLoad(path, std::forward<Args>(args)...);
    }

    /**
     * @brief Charge une ressource de manière asynchrone via le JobSystem.
     * @tparam T Type de la ressource.
     * @param callback Fonction appelée une fois le chargement terminé.
     */
    template<typename T, typename... Args>
    void loadAsync(std::string_view path, std::function<void(Ref<T>)> callback, Args&&... args) {
        std::string sPath(path);
        // On doit capturer les arguments par valeur pour l'async
        auto tupleArgs = std::make_tuple(std::forward<Args>(args)...);
        m_jobSystem.execute([this, sPath, callback, tupleArgs](std::stop_token) {
            // Note: Simplifié, le forwarding de tuple vers variadic load est complexe en C++20 sans apply
            // On va juste supporter le cas sans args pour loadAsync pour l'instant ou utiliser std::apply
            auto resource = std::apply([&](auto&&... unpackedArgs) {
                return load<T>(sPath, std::forward<decltype(unpackedArgs)>(unpackedArgs)...);
            }, tupleArgs);
            if (callback) callback(resource);
        });
    }

    /** @brief Vide tous les caches de ressources. */
    void clearCache();

private:
    template<typename T>
    ResourceCache<T>& getCache() {
        std::type_index typeIdx(typeid(T));
        {
            std::shared_lock<std::shared_mutex> readLock(m_registryMutex);
            auto it = m_caches.find(typeIdx);
            if (it != m_caches.end()) return static_cast<ResourceCache<T>&>(*it->second);
        }

        std::unique_lock<std::shared_mutex> writeLock(m_registryMutex);
        auto it = m_caches.find(typeIdx);
        if (it != m_caches.end()) return static_cast<ResourceCache<T>&>(*it->second);

        auto cache = std::make_unique<ResourceCache<T>>(m_context, this);
        auto& ref = *cache;
        m_caches[typeIdx] = std::move(cache);
        return ref;
    }

    VulkanContext& m_context;
    JobSystem& m_jobSystem;
    std::shared_mutex m_registryMutex;
    std::unordered_map<std::type_index, std::unique_ptr<IResourceCache>> m_caches;
};

} // namespace bb3d
