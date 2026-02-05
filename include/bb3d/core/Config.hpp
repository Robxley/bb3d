#pragma once

#include <string>
#include <string_view>
#include <nlohmann/json.hpp>

namespace bb3d {

    enum class PhysicsBackend { None, Jolt };

    NLOHMANN_JSON_SERIALIZE_ENUM(PhysicsBackend, {
        {PhysicsBackend::None, "none"},
        {PhysicsBackend::Jolt, "jolt"}
    })

    enum class FogType { None, Linear, Exponential, ExponentialHeight };

    /**
     * @brief Configuration de la fenêtre et de l'affichage.
     */
    struct WindowConfig {
        std::string title = "biobazard3d"; ///< Titre de la fenêtre.
        int width = 1280;   ///< Largeur en pixels.
        int height = 720;   ///< Hauteur en pixels.
        bool fullscreen = false; ///< Mode plein écran exclusif ou fenêtré sans bord (selon implémentation SDL).
        bool resizable = true;   ///< Autorise le redimensionnement par l'utilisateur.

        WindowConfig& setTitle(std::string_view t) { title = t; return *this; }
        WindowConfig& setResolution(int w, int h) { width = w; height = h; return *this; }
        WindowConfig& setFullscreen(bool f) { fullscreen = f; return *this; }
        WindowConfig& setResizable(bool r) { resizable = r; return *this; }

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(WindowConfig, title, width, height, fullscreen, resizable)
    };

    /**
     * @brief Configuration globale du pipeline graphique.
     */
    struct GraphicsConfig {
        bool vsync = true; ///< Synchronisation verticale (évite le tearing).
        int fpsMax = 0;    ///< Limite de FPS (0 = illimité). Utile pour économiser la batterie/GPU.
        std::string buffering = "triple"; ///< Stratégie de SwapChain : "double" (latence faible, risque tearing sans vsync) ou "triple" (plus fluide).
        int msaaSamples = 1; ///< Anti-aliasing MSAA (1 = désactivé, 2, 4, 8).
        float anisotropy = 16.0f; ///< Filtrage anisotrope maximal pour les textures (1.0 à 16.0).
        int shadowMapResolution = 2048; ///< Résolution des textures d'ombres (plus haut = plus net mais plus coûteux).
        bool enableValidationLayers = true; ///< Active les couches de validation Vulkan (Debug uniquement, impact perf).
        bool enableFrustumCulling = true;   ///< Active le culling des objets hors champ (Optimisation CPU).
        bool enableMipmapping = true;       ///< Génération automatique des mips pour les textures.
        bool enableOffscreenRendering = false; ///< Rendu dans une texture intermédiaire (ex: pour post-process personnalisé).
        float renderScale = 1.0f;              ///< Échelle de résolution interne (0.5 = 50% de la taille fenêtre, 1.0 = Natif).

        GraphicsConfig& setVsync(bool v) { vsync = v; return *this; }
        GraphicsConfig& setFpsMax(int fps) { fpsMax = fps; return *this; }
        GraphicsConfig& setBuffering(std::string_view b) { buffering = b; return *this; }
        GraphicsConfig& setMsaaSamples(int s) { msaaSamples = s; return *this; }
        GraphicsConfig& setValidationLayers(bool e) { enableValidationLayers = e; return *this; }
        GraphicsConfig& setFrustumCulling(bool e) { enableFrustumCulling = e; return *this; }
        GraphicsConfig& setMipmapping(bool e) { enableMipmapping = e; return *this; }
        GraphicsConfig& setOffscreenRendering(bool e) { enableOffscreenRendering = e; return *this; }
        GraphicsConfig& setRenderScale(float s) { renderScale = s; return *this; }

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(GraphicsConfig, vsync, fpsMax, buffering, msaaSamples, anisotropy, shadowMapResolution, enableValidationLayers, enableFrustumCulling, enableMipmapping, enableOffscreenRendering, renderScale)
    };

    /**
     * @brief Configuration du rasterizer.
     */
    struct RasterizerConfig {
        std::string cullMode = "Back"; ///< "None", "Front", "Back", "FrontAndBack".
        std::string frontFace = "CCW"; ///< "CW", "CCW".
        std::string polygonMode = "Fill"; ///< "Fill", "Line", "Point".

        RasterizerConfig& setCullMode(std::string_view m) { cullMode = m; return *this; }
        RasterizerConfig& setPolygonMode(std::string_view m) { polygonMode = m; return *this; }

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(RasterizerConfig, cullMode, frontFace, polygonMode)
    };

    /**
     * @brief Configuration du Depth et Stencil buffer.
     */
    struct DepthStencilConfig {
        bool depthTest = true;
        bool depthWrite = true;
        std::string depthCompareOp = "Less"; ///< "Less", "LessOrEqual", etc.
        bool stencilTest = false;

        DepthStencilConfig& setDepthTest(bool t) { depthTest = t; return *this; }
        DepthStencilConfig& setDepthWrite(bool w) { depthWrite = w; return *this; }

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(DepthStencilConfig, depthTest, depthWrite, depthCompareOp, stencilTest)
    };

    /**
     * @brief Activation/Désactivation des modules optionnels (Pay for what you use).
     */
    struct ModuleConfig {
        PhysicsBackend physicsBackend = PhysicsBackend::Jolt; ///< Moteur physique à utiliser.
        bool enablePhysics = true;    ///< Active l'initialisation du monde physique et des RigidBodies.
        bool enableAudio = true;      ///< Active le système audio (miniaudio/OpenAL).
        bool enableJobSystem = true;  ///< Active le système de threads worker.
        bool enableHotReload = true;  ///< Active le rechargement à chaud des assets (Dev Only).

        ModuleConfig& setPhysics(bool e, PhysicsBackend b = PhysicsBackend::Jolt) { enablePhysics = e; physicsBackend = b; return *this; }
        ModuleConfig& setAudio(bool e) { enableAudio = e; return *this; }
        ModuleConfig& setJobSystem(bool e) { enableJobSystem = e; return *this; }

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(ModuleConfig, physicsBackend, enablePhysics, enableAudio, enableJobSystem, enableHotReload)
    };

    /**
     * @brief Configuration système bas niveau et chemins.
     */
    struct SystemConfig {
        int maxThreads = 8; ///< Nombre de threads pour le JobSystem (généralement nb_cores - 1).
        std::string assetPath = "assets"; ///< Chemin racine des assets.
        std::string logLevel = "Info";    ///< Niveau de log par défaut.
        
        bool logConsole = true; ///< Active la sortie console.
        bool logFile = true;    ///< Active la sortie fichier.
        std::string logDirectory = "logs"; ///< Dossier de stockage des logs.

        SystemConfig& setMaxThreads(int t) { maxThreads = t; return *this; }
        SystemConfig& setAssetPath(std::string_view p) { assetPath = p; return *this; }

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(SystemConfig, maxThreads, assetPath, logLevel, logConsole, logFile, logDirectory)
    };

    /**
     * @brief Structure racine regroupant toute la configuration du moteur.
     */
    struct EngineConfig {
        WindowConfig window;
        GraphicsConfig graphics;
        RasterizerConfig rasterizer;
        DepthStencilConfig depthStencil;
        ModuleConfig modules;
        SystemConfig system;

        // --- Fluent API Helpers ---
        EngineConfig& title(std::string_view t) { window.setTitle(t); return *this; }
        EngineConfig& resolution(int w, int h) { window.setResolution(w, h); return *this; }
        EngineConfig& vsync(bool v) { graphics.setVsync(v); return *this; }
        EngineConfig& fpsMax(int f) { graphics.setFpsMax(f); return *this; }
        EngineConfig& enablePhysics(PhysicsBackend b) { modules.setPhysics(b != PhysicsBackend::None, b); return *this; }
        EngineConfig& enableAudio(bool e) { modules.setAudio(e); return *this; }
        EngineConfig& enableJobSystem(bool e) { modules.setJobSystem(e); return *this; }
        EngineConfig& frustumCulling(bool e) { graphics.setFrustumCulling(e); return *this; }
        EngineConfig& mipmapping(bool e) { graphics.setMipmapping(e); return *this; }
        EngineConfig& resizable(bool r) { window.setResizable(r); return *this; }
        EngineConfig& enableOffscreenRendering(bool e) { graphics.setOffscreenRendering(e); return *this; }
        EngineConfig& renderScale(float s) { graphics.setRenderScale(s); return *this; }
        EngineConfig& frontFace(std::string_view f) { rasterizer.frontFace = f; return *this; } // "CW" ou "CCW"

        /// @name Layout Locations par défaut pour les Shaders
        /// @{
        static constexpr uint32_t LAYOUT_LOCATION_POSITION = 0;
        static constexpr uint32_t LAYOUT_LOCATION_NORMAL   = 1;
        static constexpr uint32_t LAYOUT_LOCATION_COLOR    = 2;
        static constexpr uint32_t LAYOUT_LOCATION_TEXCOORD = 3;
        static constexpr uint32_t LAYOUT_LOCATION_TANGENT  = 4;
        static constexpr uint32_t LAYOUT_LOCATION_JOINTS   = 5;
        static constexpr uint32_t LAYOUT_LOCATION_WEIGHTS  = 6;
        /// @}

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(EngineConfig, window, graphics, rasterizer, depthStencil, modules, system)
    };

    /**
     * @brief Classe utilitaire pour charger et sauvegarder la configuration.
     */
    class Config {
    public:
        /**
         * @brief Charge la configuration depuis un fichier JSON.
         * @param path Chemin du fichier.
         * @return EngineConfig Structure chargée (ou valeurs par défaut en cas d'erreur).
         */
        static EngineConfig Load(std::string_view path);

        /**
         * @brief Sauvegarde la configuration dans un fichier JSON.
         * @param path Chemin du fichier.
         * @param config Structure à sauvegarder.
         */
        static void Save(std::string_view path, const EngineConfig& config);
    };

} // namespace bb3d