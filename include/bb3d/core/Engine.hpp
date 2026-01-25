#pragma once

#include "bb3d/core/Base.hpp"
#include "bb3d/core/Config.hpp"
#include "bb3d/core/Window.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/resource/ResourceManager.hpp"
#include "bb3d/core/JobSystem.hpp"
#include "bb3d/core/EventBus.hpp"
#include "bb3d/input/InputManager.hpp"
#include "bb3d/scene/Scene.hpp"

#include <string_view>
#include <string>

namespace bb3d {

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
    void Stop();

    /** @name Accesseurs Haut Niveau (Aliases)
     * @{
     */
    inline VulkanContext& graphics() { return *m_VulkanContext; }
    inline ResourceManager& assets() { return *m_ResourceManager; }
    inline Window& window() { return *m_Window; }
    inline JobSystem& jobs() { return *m_JobSystem; }
    inline EventBus& events() { return *m_EventBus; }
    inline InputManager& input() { return *m_InputManager; }
    /** @} */

    /** @name Accesseurs Originaux
     * @{
     */
    VulkanContext& GetVulkanContext() { return *m_VulkanContext; }
    ResourceManager& GetResourceManager() { return *m_ResourceManager; }
    Window& GetWindow() { return *m_Window; }
    JobSystem* GetJobSystem() { return m_JobSystem.get(); }
    EventBus& GetEventBus() { return *m_EventBus; }
    /** @} */

    Ref<Scene> CreateScene();
    void SetActiveScene(Ref<Scene> scene) { m_ActiveScene = scene; }
    Ref<Scene> GetActiveScene() const { return m_ActiveScene; }

private:
    void Init();
    void Shutdown();
    void Update(float deltaTime);
    void Render();

    static Engine* s_Instance;

    EngineConfig m_Config;
    Scope<Window> m_Window;
    Scope<VulkanContext> m_VulkanContext;
    Scope<ResourceManager> m_ResourceManager;
    Scope<JobSystem> m_JobSystem;
    Scope<EventBus> m_EventBus;
    Scope<InputManager> m_InputManager;
    Ref<Scene> m_ActiveScene;

    bool m_Running = false;
};
} // namespace bb3d