#pragma once

#include "bb3d/core/Base.hpp"
#include "bb3d/core/Config.hpp"
#include "bb3d/core/Window.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/resource/ResourceManager.hpp"
#include "bb3d/core/JobSystem.hpp"
#include "bb3d/core/EventBus.hpp"
#include "bb3d/input/InputManager.hpp"
#include "bb3d/render/Renderer.hpp"
#include "bb3d/scene/Scene.hpp"

#include <string_view>
#include <string>

namespace bb3d {

class PhysicsWorld;
class AudioSystem;
class ImGuiLayer;

/**
 * @brief The main class representing the biobazard3d game engine.
 * 
 * This class acts as a facade for all sub-systems
 * (Rendering, Audio, Physics, Resources) and manages the main game loop.
 */
class Engine {
public:
    /**
     * @brief Constructs a new instance of Engine.
     * @param configPath Path to the JSON configuration file.
     */
    Engine(const std::string_view configPath = "config/engine_config.json");
    
    /** @brief Constructs an instance with a direct configuration. */
    Engine(const EngineConfig& config);

    /**
     * @brief Destroys the Engine instance and releases resources.
     */
    ~Engine();

    /**
     * @brief Creates and initializes a global instance of the engine.
     */
    static Scope<Engine> Create(const std::string_view configPath = "config/engine_config.json");
    static Scope<Engine> Create(const EngineConfig& config = EngineConfig());

    /**
     * @brief Retrieves the engine singleton instance.
     */
    static Engine& Get();

    /**
     * @brief Starts the main loop of the engine.
     * 
     * This method blocks execution until the window is closed
     * or Stop() is called.
     * 
     * Sequence: Input -> Window Events -> Update -> Render.
     */
    void Run();

    /**
     * @brief Cleanly shuts down the engine and releases resources.
     * 
     * Automatically called by the destructor, but can be called manually.
     * Waits for the GPU to be idle before destroying Vulkan objects.
     */
    void Shutdown();

    // Core system accessors
    
    /**
     * @brief Requests the main loop to stop.
     * 
     * The engine will stop at the end of the current frame.
     */
    void Stop();

    /** @name High-Level Accessors (Aliases)
     * @{
     */
    inline VulkanContext& graphics() { return *m_VulkanContext; }
    inline Renderer& renderer() { return *m_Renderer; }
    inline ResourceManager& assets() { return *m_ResourceManager; }
    inline Window& window() { return *m_Window; }
    inline JobSystem& jobs() { return *m_JobSystem; }
    inline EventBus& events() { return *m_EventBus; }
    inline InputManager& input() { return *m_InputManager; }
    inline PhysicsWorld& physics() { return *m_PhysicsWorld; }
    inline AudioSystem& audio() { return *m_AudioSystem; }
#if defined(BB3D_ENABLE_EDITOR)
    inline ImGuiLayer& editor() { return *m_ImGuiLayer; }
#endif
    /** @} */

    /** @name Original Accessors
     * @{
     */
    VulkanContext& GetVulkanContext() { return *m_VulkanContext; }
    Renderer& GetRenderer() { return *m_Renderer; }
    ResourceManager& GetResourceManager() { return *m_ResourceManager; }
    Window& GetWindow() { return *m_Window; }
    JobSystem* GetJobSystem() { return m_JobSystem.get(); }
    EventBus& GetEventBus() { return *m_EventBus; }
    PhysicsWorld* GetPhysicsWorld() { return m_PhysicsWorld.get(); }
    AudioSystem* GetAudioSystem() { return m_AudioSystem.get(); }
#if defined(BB3D_ENABLE_EDITOR)
    ImGuiLayer* GetImGuiLayer() { return m_ImGuiLayer.get(); }
#endif
    /** @} */

    Ref<Scene> CreateScene();
    void SetActiveScene(Ref<Scene> scene) { m_ActiveScene = scene; }
    Ref<Scene> GetActiveScene() const { return m_ActiveScene; }

    const EngineConfig& GetConfig() const { return m_Config; }

    void exportScene(const std::string& filepath);
    void importScene(const std::string& filepath);

    /** @brief Pauses or resumes the physical simulation. */
    void setPhysicsPaused(bool paused);
    /** @brief Indicates if the physical simulation is paused. */
    bool isPhysicsPaused() const { return m_PhysicsPaused; }

    /** @brief Resets the current scene (object positions, physics). */
    void resetScene();

private:
    void Init();
    void Update(float deltaTime);
    void Render();

    static Engine* s_Instance;

    EngineConfig m_Config;
    Scope<Window> m_Window;
    Scope<VulkanContext> m_VulkanContext;
    Scope<Renderer> m_Renderer;
    Scope<ResourceManager> m_ResourceManager;
    Scope<JobSystem> m_JobSystem;
    Scope<EventBus> m_EventBus;
    Scope<InputManager> m_InputManager;
    Scope<PhysicsWorld> m_PhysicsWorld;
    Scope<AudioSystem> m_AudioSystem;
#if defined(BB3D_ENABLE_EDITOR)
    Scope<ImGuiLayer> m_ImGuiLayer;
#endif
    Ref<Scene> m_ActiveScene;

    bool m_Running = false;
    bool m_PhysicsPaused = false;
};
} // namespace bb3d