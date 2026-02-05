#include "bb3d/core/Engine.hpp"
#include "bb3d/core/Log.hpp"
#include "bb3d/physics/PhysicsWorld.hpp"
#include "bb3d/audio/AudioSystem.hpp"
#include "bb3d/scene/SceneSerializer.hpp"
#include "bb3d/render/Material.hpp"
#include <SDL3/SDL.h>
#include <stdexcept>

namespace bb3d {

Engine* Engine::s_Instance = nullptr;

Engine& Engine::Get() {
    if (!s_Instance) {
        throw std::runtime_error("Engine instance is null! Did you create it?");
    }
    return *s_Instance;
}

Engine::Engine(const std::string_view configPath) {
    if (s_Instance) {
        throw std::runtime_error("Engine instance already exists!");
    }
    s_Instance = this;

    m_Config = Config::Load(configPath);
    Log::Init(m_Config);
    BB_CORE_INFO("Engine: Initializing biobazard3d...");

    Init();
}

Engine::Engine(const EngineConfig& config) : m_Config(config) {
    if (s_Instance) {
        throw std::runtime_error("Engine instance already exists!");
    }
    s_Instance = this;

    Log::Init(m_Config);
    BB_CORE_INFO("Engine: Initializing biobazard3d from memory config...");
    Init();
}

Scope<Engine> Engine::Create(const EngineConfig& config) {
    return CreateScope<Engine>(config);
}

Engine::~Engine() {
    Shutdown();
    s_Instance = nullptr;
}

void Engine::Init() {
    BB_PROFILE_SCOPE("Engine::Init");

    // 1. Job System (Initialisé en premier pour être dispo pour les chargements asynchrones)
    if (m_Config.modules.enableJobSystem) {
        m_JobSystem = CreateScope<JobSystem>();
        m_JobSystem->init(m_Config.system.maxThreads);
    } else {
        BB_CORE_WARN("Engine: JobSystem is disabled in config.");
    }

    // 2. Event Bus (Communication découplée entre systèmes)
    m_EventBus = CreateScope<EventBus>();

    // 3. Input Manager (Doit être prêt avant la création de la fenêtre pour les callbacks)
    m_InputManager = CreateScope<InputManager>();

    // 4. Window (SDL)
    // Nécessaire pour créer la surface Vulkan par la suite.
    m_Window = CreateScope<Window>(m_Config);
    m_Window->SetEventCallback([this](SDL_Event& e) {
        if (m_InputManager) m_InputManager->onEvent(e);
        
        // Gestion du redimensionnement : On notifie le renderer pour reconstruire la SwapChain
        if (e.type == SDL_EVENT_WINDOW_RESIZED || e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
            int w = e.window.data1;
            int h = e.window.data2;
            if (w > 0 && h > 0 && m_Renderer) {
                m_Renderer->onResize(w, h);
            }
        }
    });

    // 5. Vulkan Context (Instance, PhysicalDevice, LogicalDevice)
    // Initialise Vulkan en s'attachant à la fenêtre SDL créée juste avant.
    m_VulkanContext = CreateScope<VulkanContext>();
    m_VulkanContext->init(m_Window->GetNativeWindow(), m_Config.window.title, m_Config.graphics.enableValidationLayers);

    // 6. Renderer (SwapChain, Pipelines, RenderPasses)
    // Dépend du VulkanContext et de la Window.
    m_Renderer = CreateScope<Renderer>(*m_VulkanContext, *m_Window, m_Config);

    // 7. Resource Manager (Textures, Models, Shaders)
    // Gère le chargement et le cache. Utilise le JobSystem pour le chargement async.
    m_ResourceManager = CreateScope<ResourceManager>(*m_VulkanContext, *m_JobSystem);

    // 8. Physics (Jolt)
    // Indépendant du rendu, peut être initialisé ici.
    if (m_Config.modules.enablePhysics) {
        m_PhysicsWorld = CreateScope<PhysicsWorld>();
        m_PhysicsWorld->init();
    }

    // 9. Audio (miniaudio/OpenAL)
    if (m_Config.modules.enableAudio) {
        m_AudioSystem = CreateScope<AudioSystem>();
        m_AudioSystem->init();
    }

    BB_CORE_INFO("Engine: Initialization complete.");
}

void Engine::Shutdown() {
    BB_PROFILE_SCOPE("Engine::Shutdown");
    BB_CORE_INFO("Engine: Shutting down...");

    // 1. Attente GPU : On s'assure que le GPU ne travaille plus avant de détruire quoi que ce soit.
    if (m_VulkanContext && m_VulkanContext->getDevice()) {
        m_VulkanContext->getDevice().waitIdle();
    }

    // 2. Systèmes de haut niveau (Audio, Physique)
    if (m_AudioSystem) {
        m_AudioSystem->shutdown();
        m_AudioSystem.reset();
    }

    if (m_PhysicsWorld) {
        m_PhysicsWorld->shutdown();
        m_PhysicsWorld.reset();
    }

    // 3. Scène active (Détruit les Entités, Components, Scripts)
    m_ActiveScene.reset();
    
    // 4. Renderer
    // IMPORTANT : On détruit le Renderer avant le Context et le Window.
    // Il contient la SwapChain et les Framebuffers qui dépendent de la Surface.
    m_Renderer.reset(); 

    // 5. Ressources
    // On vide le cache des textures/modèles. Note : Les textures Vulkan dépendent de l'allocator (VMA)
    // qui est géré par le VulkanContext. Elles doivent être libérées AVANT le contexte.
    if (m_ResourceManager) {
        m_ResourceManager->clearCache();
        m_ResourceManager.reset();
    }
    
    Material::Cleanup(); // Libérer les textures "par défaut" (blanc, noir)

    // 6. Bas niveau (Vulkan, Window)
    m_VulkanContext.reset(); // Détruit Device, Instance, Surface.
    m_Window.reset(); // Ferme la fenêtre SDL.
    m_EventBus.reset();

    // 7. JobSystem (On arrête les threads en dernier)
    if (m_JobSystem) {
        m_JobSystem->shutdown();
        m_JobSystem.reset();
    }
}

void Engine::Run() {
    m_Running = true;
    BB_CORE_INFO("Engine: Entering main loop.");

    uint64_t lastTime = SDL_GetPerformanceCounter();
    uint64_t frequency = SDL_GetPerformanceFrequency();

    while (m_Running && !m_Window->ShouldClose()) {
        BB_PROFILE_FRAME("MainLoop");

        uint64_t currentTime = SDL_GetPerformanceCounter();
        float deltaTime = static_cast<float>(currentTime - lastTime) / static_cast<float>(frequency);
        lastTime = currentTime;

        if (m_InputManager) m_InputManager->update();
        m_Window->PollEvents();
        
        Update(deltaTime);
        Render();
    }

    BB_CORE_INFO("Engine: Main loop exited.");
}

void Engine::Stop() {
    m_Running = false;
}

Ref<Scene> Engine::CreateScene() {
    auto scene = CreateRef<Scene>();
    scene->setEngineContext(this);
    return scene;
}

void Engine::Update(float deltaTime) {
    BB_PROFILE_SCOPE("Engine::Update");
    
    if (m_EventBus) {
        m_EventBus->dispatchQueued();
    }

    if (m_PhysicsWorld && m_ActiveScene) {
        m_PhysicsWorld->update(deltaTime, *m_ActiveScene);
    }

    if (m_AudioSystem) {
        m_AudioSystem->update(deltaTime);
    }

    if (m_ActiveScene) {
        m_ActiveScene->onUpdate(deltaTime);
    }
}

void Engine::Render() {
    BB_PROFILE_SCOPE("Engine::Render");
    
    if (m_ActiveScene && m_Renderer) {
        m_Renderer->render(*m_ActiveScene);
    }
}

void Engine::exportScene(const std::string& filepath) {
    if (m_ActiveScene) {
        SceneSerializer serializer(m_ActiveScene);
        serializer.serialize(filepath);
    }
}

void Engine::importScene(const std::string& filepath) {
    auto scene = CreateScene();
    SceneSerializer serializer(scene);
    if (serializer.deserialize(filepath)) {
        SetActiveScene(scene);
    }
}

} // namespace bb3d