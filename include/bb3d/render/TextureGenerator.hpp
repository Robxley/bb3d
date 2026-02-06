#pragma once

#include "bb3d/render/Texture.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include <string_view>

namespace bb3d {

class TextureGenerator {
public:
    /**
     * @brief Combine 3 textures (AO, Roughness, Metallic) en une seule texture ORM.
     * 
     * @param context Contexte Vulkan pour créer la texture finale.
     * @param aoPath Chemin vers la map d'Ambient Occlusion (R channel). Peut être vide (blanc par défaut).
     * @param roughnessPath Chemin vers la map de Roughness (G channel). Peut être vide (blanc par défaut).
     * @param metallicPath Chemin vers la map de Metallic (B channel). Peut être vide (noir par défaut).
     * @return Ref<Texture> La texture combinée ORM.
     */
    static Ref<Texture> combineORM(VulkanContext& context, std::string_view aoPath, std::string_view roughnessPath, std::string_view metallicPath);
};

} // namespace bb3d
