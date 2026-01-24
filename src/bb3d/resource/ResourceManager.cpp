#include "bb3d/resource/ResourceManager.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/core/Log.hpp"

namespace bb3d {

ResourceManager::ResourceManager(VulkanContext& context, JobSystem& jobSystem) 
    : m_context(context), m_jobSystem(jobSystem) {
}

void ResourceManager::clearCache() {
    // Lock exclusif pour nettoyer la map des caches
    std::unique_lock<std::shared_mutex> lock(m_registryMutex);
    
    for (auto& [type, cache] : m_caches) {
        cache->clear();
    }
    
    BB_CORE_INFO("ResourceManager: Tous les caches ont été vidés.");
}

} // namespace bb3d
