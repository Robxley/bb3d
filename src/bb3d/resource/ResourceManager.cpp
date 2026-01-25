#include "bb3d/resource/ResourceManager.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/core/Log.hpp"

namespace bb3d {

ResourceManager::ResourceManager(VulkanContext& context, JobSystem& jobSystem) 
    : m_context(context), m_jobSystem(jobSystem) {
}

void ResourceManager::clearCache() {
    std::unique_lock<std::shared_mutex> lock(m_registryMutex);
    BB_CORE_TRACE("ResourceManager: Clearing {} resource caches...", m_caches.size());
    for (auto& [type, cache] : m_caches) {
        cache->clear();
    }
    m_caches.clear();
}

} // namespace bb3d
