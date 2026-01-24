#pragma once

#include "bb3d/core/Base.hpp"
#include <string>

namespace bb3d {

/**
 * @brief Classe de base pour toutes les ressources (Texture, Model, Shader, etc.)
 */
class Resource {
public:
    Resource() = default;
    explicit Resource(const std::string& path) : m_path(path), m_loaded(true) {}
    virtual ~Resource() = default;

    const std::string& getPath() const { return m_path; }
    bool isLoaded() const { return m_loaded; }

protected:
    std::string m_path;
    bool m_loaded = false;
};

} // namespace bb3d
