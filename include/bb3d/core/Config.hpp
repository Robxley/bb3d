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

    struct WindowConfig {
        std::string title = "biobazard3d";
        int width = 1280;
        int height = 720;
        bool fullscreen = false;
        bool resizable = true;

        WindowConfig& setTitle(std::string_view t) { title = t; return *this; }
        WindowConfig& setResolution(int w, int h) { width = w; height = h; return *this; }
        WindowConfig& setFullscreen(bool f) { fullscreen = f; return *this; }
        WindowConfig& setResizable(bool r) { resizable = r; return *this; }

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
        bool enableFrustumCulling = true;
        bool enableMipmapping = true;
        bool enableOffscreenRendering = false; // Rendu dans une texture intermédiaire
        float renderScale = 1.0f;              // Échelle de la résolution interne (0.5 = 50%, 1.0 = Natif)

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
     * @brief Activation/Désactivation des modules optionnels.
     */
    struct ModuleConfig {
        PhysicsBackend physicsBackend = PhysicsBackend::Jolt;
        bool enablePhysics = true;
        bool enableAudio = true;
        bool enableJobSystem = true;
        bool enableHotReload = true;

        ModuleConfig& setPhysics(bool e, PhysicsBackend b = PhysicsBackend::Jolt) { enablePhysics = e; physicsBackend = b; return *this; }
        ModuleConfig& setAudio(bool e) { enableAudio = e; return *this; }
        ModuleConfig& setJobSystem(bool e) { enableJobSystem = e; return *this; }

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(ModuleConfig, physicsBackend, enablePhysics, enableAudio, enableJobSystem, enableHotReload)
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