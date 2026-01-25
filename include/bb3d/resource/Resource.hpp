#pragma once

#include "bb3d/core/Base.hpp"
#include <string>

namespace bb3d {

/**
 * @brief Classe de base pour toutes les ressources gérées par le ResourceManager.
 * 
 * Les ressources sont identifiées par leur chemin d'accès et possèdent un état de chargement.
 */
class Resource {
public:
    Resource() = default;
    explicit Resource(const std::string& path) : m_path(path), m_loaded(true) {}
    virtual ~Resource() = default;

    /** @brief Récupère le chemin source de la ressource. */
    [[nodiscard]] inline const std::string& getPath() const { return m_path; }
    
    /** @brief Indique si la ressource est prête à l'emploi. */
    [[nodiscard]] inline bool isLoaded() const { return m_loaded; }

protected:
    std::string m_path;
    bool m_loaded = false;
};

} // namespace bb3d