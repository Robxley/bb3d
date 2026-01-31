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

/**
 * @brief La classe principale représentant le moteur de jeu biobazard3d.
 * 
 * Cette classe agit comme une façade (Facade Pattern) pour tous les sous-systèmes
 * (Rendu, Audio, Physique, Ressources) et gère la boucle principale du jeu.
 */
class Engine {
public:
    /**
     * @brief Construit une nouvelle instance de Engine.
     * @param configPath Chemin vers le fichier de configuration JSON.
     */
    Engine(const std::string_view configPath = "engine_config.json");
    
    /** @brief Construit une instance avec une config directe. */
    Engine(const EngineConfig& config);

    /**
     * @brief Détruit l'instance de Engine et libère les ressources.
     */
    ~Engine();

    /**
     * @brief Crée et initialise une instance globale du moteur.
     */
    static Scope<Engine> Create(const EngineConfig& config = EngineConfig());

    /**
     * @brief Récupère l'instance singleton du moteur.
     */
    static Engine& Get();

    void Run();
    void Shutdown();

    // Accesseurs aux systèmes core
    void Stop();

    /** @name Accesseurs Haut Niveau (Aliases)
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
    /** @} */

    /** @name Accesseurs Originaux
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
    /** @} */

    Ref<Scene> CreateScene();
    void SetActiveScene(Ref<Scene> scene) { m_ActiveScene = scene; }
    Ref<Scene> GetActiveScene() const { return m_ActiveScene; }

    void exportScene(const std::string& filepath);
    void importScene(const std::string& filepath);

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
    Ref<Scene> m_ActiveScene;

    bool m_Running = false;
};
} // namespace bb3d