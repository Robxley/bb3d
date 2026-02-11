#include "bb3d/core/Engine.hpp"
#include "bb3d/core/Log.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/physics/PhysicsWorld.hpp"
#include "bb3d/audio/AudioSystem.hpp"
#include "bb3d/scene/SceneSerializer.hpp"
#include "bb3d/render/Material.hpp"
#if defined(BB3D_ENABLE_EDITOR)
#include "bb3d/core/ImGuiLayer.hpp"
#endif
#include <SDL3/SDL.h>
#include <stdexcept>
#include <fstream>

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
#if defined(BB3D_ENABLE_EDITOR)
        if (m_ImGuiLayer) m_ImGuiLayer->onEvent(e);
#endif
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
    m_Renderer = CreateScope<Renderer>(*m_VulkanContext, *m_Window, *m_JobSystem, m_Config);

    // 6.5 ImGui Layer (Editor UI)
#if defined(BB3D_ENABLE_EDITOR)
    m_ImGuiLayer = CreateScope<ImGuiLayer>(*m_VulkanContext, *m_Window);
#endif

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

    // On attend que le GPU ait fini son travail avant de tout péter
    if (m_VulkanContext && m_VulkanContext->getDevice()) {
        try {
            m_VulkanContext->getDevice().waitIdle();
        } catch(...) {}
    }

    // 1. Audio
    if (m_AudioSystem) {
        m_AudioSystem->shutdown();
        m_AudioSystem.reset();
    }

    // 2. Physique
    if (m_PhysicsWorld) {
        m_PhysicsWorld->shutdown();
        m_PhysicsWorld.reset();
    }

    // 3. Scène active (Détruit les Entités, Components, Scripts)
    // IMPORTANT : On vide le registre AVANT le renderer
    if (m_ActiveScene) {
        m_ActiveScene->getRegistry().clear();
    }
    m_ActiveScene.reset();

#if defined(BB3D_ENABLE_EDITOR)
    m_ImGuiLayer.reset();
#endif
    
    // 4. Renderer
    // IMPORTANT : On détruit le Renderer avant le Context et le Window.
    // Il contient la SwapChain et les Framebuffers qui dépendent de la Surface.
    m_Renderer.reset(); 

    // Deuxième waitIdle par sécurité après destruction du renderer
    if (m_VulkanContext && m_VulkanContext->getDevice()) {
        try {
            m_VulkanContext->getDevice().waitIdle();
        } catch(...) {}
    }

    // 5. Ressources
    // On vide le cache des textures/modèles. Note : Les textures Vulkan dépendent de l'allocator (VMA)
    // qui est géré par le VulkanContext. Elles doivent être libérées AVANT le contexte.
    if (m_ResourceManager) {
        m_ResourceManager->clearCache();
        m_ResourceManager.reset();
    }

    // Libérer les ressources statiques de Material avant de détruire l'allocateur
    Material::Cleanup();

    // 6. Contexte Graphique
    m_VulkanContext.reset();

    // 7. Fenêtre
    m_Window.reset();

    // 8. Communication & Input
    m_EventBus.reset();
    m_InputManager.reset();

    // 9. Système de Jobs (en dernier pour que les threads soient dispos pour les destructeurs si besoin)
    if (m_JobSystem) {
        m_JobSystem->shutdown();
        m_JobSystem.reset();
    }

    BB_CORE_INFO("Engine: Shutdown complete.");
}

void Engine::Run() {
    m_Running = true;
    BB_CORE_INFO("Engine: Entering main loop.");

    uint64_t lastTime = SDL_GetTicks();

    while (m_Running && !m_Window->ShouldClose()) {
        BB_PROFILE_FRAME("MainLoop");

        // 0. Réinitialiser les deltas d'entrée pour la nouvelle frame
        if (m_InputManager) {
            m_InputManager->clearDeltas();
        }

        uint64_t currentTime = SDL_GetTicks();
        float deltaTime = static_cast<float>(currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;

        // Limiter le deltaTime pour éviter les explosions physiques au démarrage ou après un freeze
        if (deltaTime > 0.1f) deltaTime = 0.1f;

        // 1. Événements système (Fenêtre, Input)
        m_Window->PollEvents();
        
#if defined(BB3D_ENABLE_EDITOR)
        if (m_ImGuiLayer) m_ImGuiLayer->beginFrame();
#endif

        // Mise à jour de l'état des entrées (reset deltas)
        if (m_InputManager) {
            // Bloquer les inputs du jeu si ImGui capture
            bool captureMouse = false;
            bool captureKeyboard = false;
#if defined(BB3D_ENABLE_EDITOR)
            if (m_ImGuiLayer) {
                captureMouse = m_ImGuiLayer->wantCaptureMouse();
                captureKeyboard = m_ImGuiLayer->wantCaptureKeyboard();
            }
#endif
            m_InputManager->update(captureMouse, captureKeyboard);
        }

        // 2. Mise à jour de la logique (Update)
        Update(deltaTime);

        // 3. Rendu (Render)
        Render();
    }

    BB_CORE_INFO("Engine: Main loop exited.");
}

void Engine::setPhysicsPaused(bool paused) {
    if (m_PhysicsPaused != paused) {
        m_PhysicsPaused = paused;
        BB_CORE_INFO("Engine: Physics simulation {}", paused ? "PAUSED" : "RESUMED");
    }
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
    
    // Dispatch des événements accumulés
    if (m_EventBus) {
        m_EventBus->dispatchQueued();
    }

    if (m_ActiveScene) {
        // 1. Physique (La vérité master sur le Transform)
        if (m_PhysicsWorld && m_Config.modules.enablePhysics && !m_PhysicsPaused) {
            m_PhysicsWorld->update(deltaTime, *m_ActiveScene);
        }

        // 2. Caméras & Logique
        m_ActiveScene->onUpdate(deltaTime);
    }
}

void Engine::resetScene() {
    if (!m_ActiveScene) return;

    BB_CORE_INFO("Engine: Resetting scene...");

    // 1. Restaurer tous les transforms à leur état d'initialisation
    auto transformView = m_ActiveScene->getRegistry().view<TransformComponent>();
    for (auto entity : transformView) {
        transformView.get<TransformComponent>(entity).resetToInitial();
    }

    // 2. Réinitialiser les corps physiques (vitesses à zéro, téléportation)
    if (m_PhysicsWorld) {
        m_PhysicsWorld->resetAllBodies(*m_ActiveScene);
    }
}

void Engine::Render() {
    BB_PROFILE_SCOPE("Engine::Render");
    
    if (m_ActiveScene && m_Renderer) {
        // 1. Rendu de la scène
        m_Renderer->render(*m_ActiveScene);

#if defined(BB3D_ENABLE_EDITOR)
        // 2. Rendu de l'UI (ImGui) par dessus
        if (m_ImGuiLayer) {
            // On récupère le command buffer courant du renderer pour injecter l'UI
            // Note: Renderer::render() termine sa passe de rendu interne.
            // On a besoin d'une méthode pour dessiner l'UI dans le swapchain.
            m_Renderer->renderUI([this](vk::CommandBuffer cb) {
                m_ImGuiLayer->endFrame(cb);
            });
        }
#endif
    }
}

void Engine::exportScene(const std::string& filepath) {
    if (!m_ActiveScene) return;
    SceneSerializer serializer(m_ActiveScene);
    serializer.serialize(filepath);
}

void Engine::importScene(const std::string& filepath) {
    if (m_VulkanContext) m_VulkanContext->getDevice().waitIdle();

    auto scene = CreateScene();
    
    // Pause de la physique par défaut lors du chargement
    m_PhysicsPaused = true;

    SceneSerializer serializer(scene);
    if (serializer.deserialize(filepath)) {
        m_ActiveScene = scene;
#if defined(BB3D_ENABLE_EDITOR)
        if (m_ImGuiLayer) {
            m_ImGuiLayer->setSelectedEntity({});
        }
#endif
        // Reset automatique pour appliquer les états initiaux et synchroniser la physique
        resetScene();
        
        BB_CORE_INFO("Engine: Scene loaded successfully and physics PAUSED.");
    } else {
        BB_CORE_ERROR("Engine: Failed to load scene from {}", filepath);
    }
}

} // namespace bb3d
