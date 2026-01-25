#pragma once

#include "bb3d/core/Base.hpp"
#include "bb3d/core/Config.hpp"
#include "bb3d/core/Window.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include "bb3d/resource/ResourceManager.hpp"
#include "bb3d/core/JobSystem.hpp"
#include "bb3d/core/EventBus.hpp"
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
    
    /**
     * @brief Détruit l'instance de Engine et libère les ressources.
     */
    ~Engine();

    /**
     * @brief Récupère l'instance singleton du moteur.
     * @return Référence vers l'instance unique.
     * @throws std::runtime_error si l'instance n'a pas été créée.
     */
    static Engine& Get();

    /**
     * @brief Lance la boucle principale du moteur.
     */
    void Run();
    
    /**
     * @brief Arrête la boucle principale et prépare l'arrêt du moteur.
     */
    void Stop();

    /** @brief Récupère le contexte Vulkan. */
    VulkanContext& GetVulkanContext() { return *m_VulkanContext; }
    
    /** @brief Récupère le gestionnaire de ressources. */
    ResourceManager& GetResourceManager() { return *m_ResourceManager; }
    
    /** @brief Récupère la fenêtre principale. */
    Window& GetWindow() { return *m_Window; }
    
    /** @brief Récupère le système de jobs (peut être null si désactivé). */
    JobSystem* GetJobSystem() { return m_JobSystem.get(); }
    
    /** @brief Récupère le bus d'événements. */
    EventBus& GetEventBus() { return *m_EventBus; }

    /**
     * @brief Crée une nouvelle scène vide.
     * @return Ref (shared_ptr) vers la nouvelle scène.
     */
    Ref<Scene> CreateScene();
    
    /** @brief Définit la scène active pour le rendu et la mise à jour. */
    void SetActiveScene(Ref<Scene> scene) { m_ActiveScene = scene; }
    
    /** @brief Récupère la scène actuellement active. */
    Ref<Scene> GetActiveScene() const { return m_ActiveScene; }

private:
    /** @brief Initialise tous les sous-systèmes selon la configuration. */
    void Init();
    
    /** @brief Libère proprement tous les sous-systèmes. */
    void Shutdown();
    
    /** @brief Met à jour la logique du jeu et de la physique. */
    void Update(float deltaTime);
    
    /** @brief Enregistre et soumet les commandes de rendu. */
    void Render();

    static Engine* s_Instance;

    EngineConfig m_Config;
    Scope<Window> m_Window;
    Scope<VulkanContext> m_VulkanContext;
    Scope<ResourceManager> m_ResourceManager;
    Scope<JobSystem> m_JobSystem;
    Scope<EventBus> m_EventBus;
    Ref<Scene> m_ActiveScene;

    bool m_Running = false;
};

} // namespace bb3d