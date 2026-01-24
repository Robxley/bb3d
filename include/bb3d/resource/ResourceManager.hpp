#pragma once

#include "bb3d/core/Base.hpp"
#include "bb3d/resource/Resource.hpp"
#include "bb3d/core/JobSystem.hpp"
#include "bb3d/core/Log.hpp"

#include <unordered_map>
#include <string>
#include <string_view>
#include <shared_mutex> // C++17/20 : Read-Write Lock
#include <functional>
#include <typeindex>
#include <type_traits>

namespace bb3d {

// --- Interface interne ---
class IResourceCache {
public:
    virtual ~IResourceCache() = default;
    virtual void clear() = 0;
};

// --- Functor de hachage transparent (C++20) ---
// Permet d'utiliser string_view pour chercher dans une map<string> sans allocation
struct StringHash {
    using is_transparent = void; // Tag C++20
    size_t operator()(std::string_view sv) const {
        return std::hash<std::string_view>{}(sv);
    }
    size_t operator()(const std::string& s) const {
        return std::hash<std::string>{}(s);
    }
    size_t operator()(const char* s) const {
        return std::hash<std::string_view>{}(s);
    }
};

// Forward declaration
class VulkanContext;

// --- Cache spécialisé par type ---
template<typename T>
class ResourceCache : public IResourceCache {
public:
    ResourceCache(VulkanContext& context) : m_context(context) {}

    Ref<T> getOrLoad(std::string_view path) {
        // 1. FAST PATH
        {
            std::shared_lock<std::shared_mutex> readLock(m_mutex);
            auto it = m_resources.find(path);
            if (it != m_resources.end()) {
                BB_CORE_INFO("ResourceCache: Cache hit for '{0}'", std::string(path));
                return it->second;
            }
        } // Unlock Read

        // 2. SLOW PATH : Écriture (Exclusive Write)
        // La ressource n'existe pas, on doit la charger.
        std::unique_lock<std::shared_mutex> writeLock(m_mutex);
        
        // Double-Check : Un autre thread l'a peut-être chargée pendant qu'on attendait le writeLock
        auto it = m_resources.find(path);
        if (it != m_resources.end()) {
            return it->second;
        }

        BB_CORE_INFO("ResourceCache: Loading '{0}'...", std::string(path));
        // Chargement réel
        auto resource = CreateRef<T>();

        m_resources[std::string(path)] = resource;
        return resource;
    }

    void clear() override {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_resources.clear();
    }

private:
    VulkanContext& m_context;
    std::shared_mutex m_mutex;
    std::unordered_map<std::string, Ref<T>, StringHash, std::equal_to<>> m_resources;
};

// --- Façade Resource Manager ---
class ResourceManager {
public:
    ResourceManager(VulkanContext& context, JobSystem& jobSystem);
    ~ResourceManager() = default;

    template<typename T>
    Ref<T> load(std::string_view path) {
        return getCache<T>().getOrLoad(path);
    }

    template<typename T>
    void loadAsync(std::string_view path, std::function<void(Ref<T>)> callback) {
        std::string sPath(path);
        m_jobSystem.execute([this, sPath, callback](std::stop_token) {
            try {
                auto resource = load<T>(sPath);
                if (callback) callback(resource);
            } catch (const std::exception& e) {
                BB_CORE_ERROR("ResourceManager: Failed to load asset {} asynchronously: {}", sPath, e.what());
            }
        });
    }

    void clearCache();

private:
    template<typename T>
    ResourceCache<T>& getCache() {
        std::type_index typeIdx(typeid(T));

        {
            std::shared_lock<std::shared_mutex> readLock(m_registryMutex);
            auto it = m_caches.find(typeIdx);
            if (it != m_caches.end()) {
                return static_cast<ResourceCache<T>&>(*it->second);
            }
        }

        std::unique_lock<std::shared_mutex> writeLock(m_registryMutex);
        
        auto it = m_caches.find(typeIdx);
        if (it != m_caches.end()) {
            return static_cast<ResourceCache<T>&>(*it->second);
        }

        auto cache = std::make_unique<ResourceCache<T>>(m_context);
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