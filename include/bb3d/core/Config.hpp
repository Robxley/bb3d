#pragma once

#include <string>
#include <string_view>
#include <nlohmann/json.hpp>

namespace bb3d {

    struct WindowConfig {
        std::string title = "biobazard3d";
        int width = 1280;
        int height = 720;
        bool fullscreen = false;
        bool resizable = true;

        WindowConfig& setTitle(std::string_view t) { title = t; return *this; }
        WindowConfig& setResolution(int w, int h) { width = w; height = h; return *this; }
        WindowConfig& setFullscreen(bool f) { fullscreen = f; return *this; }

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(WindowConfig, title, width, height, fullscreen, resizable)
    };

    struct GraphicsConfig {
        bool vsync = true;
        int fpsMax = 0; ///< 0 pour illimité.
        std::string buffering = "triple"; ///< "double" ou "triple".
        int msaaSamples = 1; ///< 1, 2, 4, 8.
        float anisotropy = 16.0f;
        int shadowMapResolution = 2048;
        bool enableValidationLayers = true;

        GraphicsConfig& setVsync(bool v) { vsync = v; return *this; }
        GraphicsConfig& setFpsMax(int fps) { fpsMax = fps; return *this; }

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(GraphicsConfig, vsync, fpsMax, buffering, msaaSamples, anisotropy, shadowMapResolution, enableValidationLayers)
    };

    /**
     * @brief Configuration du rasterizer.
     */
    struct RasterizerConfig {
        std::string cullMode = "Back"; ///< "None", "Front", "Back", "FrontAndBack".
        std::string frontFace = "CCW"; ///< "CW", "CCW".
        std::string polygonMode = "Fill"; ///< "Fill", "Line", "Point".

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

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(DepthStencilConfig, depthTest, depthWrite, depthCompareOp, stencilTest)
    };

    /**
     * @brief Activation/Désactivation des modules optionnels.
     */
    struct ModuleConfig {
        bool enablePhysics = true;
        std::string physicsBackend = "jolt";
        bool enableAudio = true;
        bool enableJobSystem = true;
        bool enableHotReload = true;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(ModuleConfig, enablePhysics, physicsBackend, enableAudio, enableJobSystem, enableHotReload)
    };

    /**
     * @brief Configuration système et chemins.
     */
    struct SystemConfig {
        int maxThreads = 8;
        std::string assetPath = "assets";
        std::string logLevel = "Info";
        
        bool logConsole = true;
        bool logFile = true;
        std::string logDirectory = "logs";

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