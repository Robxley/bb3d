#pragma once

#include <memory>

namespace bb3d {

/**
 * @brief Alias pour les pointeurs intelligents partagés (Shared Pointers).
 * Utilisé pour les ressources partagées dans le moteur.
 */
template<typename T>
using Ref = std::shared_ptr<T>;

/**
 * @brief Crée un objet géré par un Ref (shared_ptr).
 */
template<typename T, typename... Args>
constexpr Ref<T> CreateRef(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

/**
 * @brief Alias pour les pointeurs intelligents uniques (Unique Pointers).
 * Utilisé pour la propriété exclusive.
 */
template<typename T>
using Scope = std::unique_ptr<T>;

/**
 * @brief Crée un objet géré par un Scope (unique_ptr).
 */
template<typename T, typename... Args>
constexpr Scope<T> CreateScope(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

} // namespace bb3d