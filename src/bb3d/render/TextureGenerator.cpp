#include "bb3d/render/TextureGenerator.hpp"
#include "bb3d/core/Log.hpp"
#include <stb_image.h>
#include <vector>
#include <algorithm>

namespace bb3d {

Ref<Texture> TextureGenerator::combineORM(VulkanContext& context, std::string_view aoPath, std::string_view roughnessPath, std::string_view metallicPath) {
    int w = 0, h = 0, c = 0;
    stbi_uc *ao = nullptr, *rough = nullptr, *metal = nullptr;

    // Helper pour charger et valider les dimensions
    auto load = [&](std::string_view path, stbi_uc*& ptr) {
        if (path.empty()) return;
        int lw, lh, lc;
        ptr = stbi_load(path.data(), &lw, &lh, &lc, 1); // On ne charge que le canal R (grayscale)
        if (ptr) {
            if (w == 0) { w = lw; h = lh; }
            else if (lw != w || lh != h) {
                BB_CORE_WARN("TextureGenerator: Dimension mismatch for '{}' ({}x{} vs {}x{}). Resizing not supported yet.", path, lw, lh, w, h);
                stbi_image_free(ptr); ptr = nullptr;
            }
        } else {
            BB_CORE_WARN("TextureGenerator: Failed to load '{}'", path);
        }
    };

    load(aoPath, ao);
    load(roughnessPath, rough);
    load(metallicPath, metal);

    if (w == 0) {
        BB_CORE_ERROR("TextureGenerator: No valid input textures found.");
        return nullptr;
    }

    // Buffer final RGBA (4 canaux)
    std::vector<unsigned char> ormPixels(w * h * 4);

    for (int i = 0; i < w * h; ++i) {
        // R = Occlusion (défaut 255/blanc)
        ormPixels[i * 4 + 0] = ao ? ao[i] : 255;
        // G = Roughness (défaut 255/blanc -> très rugueux)
        ormPixels[i * 4 + 1] = rough ? rough[i] : 255;
        // B = Metallic (défaut 0/noir -> diélectrique)
        ormPixels[i * 4 + 2] = metal ? metal[i] : 0;
        // A = Unused (255)
        ormPixels[i * 4 + 3] = 255;
    }

    // Cleanup
    if (ao) stbi_image_free(ao);
    if (rough) stbi_image_free(rough);
    if (metal) stbi_image_free(metal);

    BB_CORE_INFO("TextureGenerator: Generated ORM texture ({}x{})", w, h);
    
    // Création de la texture via le constructeur existant qui prend un std::span de bytes
    return CreateRef<Texture>(context, std::as_bytes(std::span(ormPixels)), w, h, false); // false = format linéaire (UNORM) pour les data maps
}

} // namespace bb3d
