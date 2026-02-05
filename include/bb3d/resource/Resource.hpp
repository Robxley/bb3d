#pragma once

#include "bb3d/core/Base.hpp"
#include <string>

namespace bb3d {

/**
 * @brief Classe de base abstraite pour toutes les ressources gérées par le ResourceManager.
 * 
 * Toutes les ressources (Textures, Modèles, Shaders) doivent dériver de cette classe.
 * Elle fournit un identifiant unique basé sur le chemin du fichier et un flag d'état.
 */
class Resource {
public:
    Resource() = default;
    
    /**
     * @brief Constructeur de ressource.
     * @param path Chemin d'accès au fichier source.
     */
    explicit Resource(const std::string& path) : m_path(path), m_loaded(true) {}
    virtual ~Resource() = default;

    /** @brief Récupère le chemin source original de la ressource. */
    [[nodiscard]] inline const std::string& getPath() const { return m_path; }
    
    /** @brief Indique si la ressource a été chargée avec succès et est prête. */
    [[nodiscard]] inline bool isLoaded() const { return m_loaded; }

protected:
    std::string m_path; ///< Chemin relatif ou absolu vers l'asset.
    bool m_loaded = false; ///< État de validité de la ressource.
};

} // namespace bb3d