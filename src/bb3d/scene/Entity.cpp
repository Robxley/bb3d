#include "bb3d/scene/Entity.hpp"
#include "bb3d/scene/Scene.hpp"

namespace bb3d {
    // La plupart des méthodes sont templates ou inline dans Entity.hpp

    std::string Entity::getName() const {
        if (*this && has<TagComponent>()) {
            return get<TagComponent>().tag;
        }
        return "Unnamed Entity";
    }

    bool Entity::isValid() const {
        return *this && m_scene->getRegistry().valid(m_entityHandle);
    }
}
