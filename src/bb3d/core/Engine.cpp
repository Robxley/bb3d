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

    // 1. Job System
    if (m_Config.modules.enableJobSystem) {
        m_JobSystem = CreateScope<JobSystem>();
        m_JobSystem->init(m_Config.system.maxThreads);
    } else {
        BB_CORE_WARN("Engine: JobSystem is disabled in config.");
    }

    // 2. Event Bus
    m_EventBus = CreateScope<EventBus>();

    // 3. Input Manager
    m_InputManager = CreateScope<InputManager>();

    // 4. Window
    m_Window = CreateScope<Window>(m_Config);
    m_Window->SetEventCallback([this](SDL_Event& e) {
        if (m_InputManager) m_InputManager->onEvent(e);
    });

    // 5. Vulkan Context
    m_VulkanContext = CreateScope<VulkanContext>();
    m_VulkanContext->init(m_Window->GetNativeWindow(), m_Config.window.title, m_Config.graphics.enableValidationLayers);

    // 6. Renderer
    m_Renderer = CreateScope<Renderer>(*m_VulkanContext, *m_Window, m_Config);

    // 7. Resource Manager
    m_ResourceManager = CreateScope<ResourceManager>(*m_VulkanContext, *m_JobSystem);

    // 8. Physics
    if (m_Config.modules.enablePhysics) {
        m_PhysicsWorld = CreateScope<PhysicsWorld>();
        m_PhysicsWorld->init();
    }

    // 9. Audio
    if (m_Config.modules.enableAudio) {
        m_AudioSystem = CreateScope<AudioSystem>();
        m_AudioSystem->init();
    }

    BB_CORE_INFO("Engine: Initialization complete.");
}

void Engine::Shutdown() {
    BB_PROFILE_SCOPE("Engine::Shutdown");
    BB_CORE_INFO("Engine: Shutting down...");

    if (m_VulkanContext && m_VulkanContext->getDevice()) {
        m_VulkanContext->getDevice().waitIdle();
    }

    if (m_AudioSystem) {
        m_AudioSystem->shutdown();
        m_AudioSystem.reset();
    }

    if (m_PhysicsWorld) {
        m_PhysicsWorld->shutdown();
        m_PhysicsWorld.reset();
    }

    m_ActiveScene.reset();
    
    m_Renderer.reset(); // On détruit le Renderer d'abord car il possède bcp de ressources VMA

    if (m_ResourceManager) {
        m_ResourceManager->clearCache();
        m_ResourceManager.reset();
    }
    
    Material::Cleanup(); // Libérer les textures par défaut

    m_VulkanContext.reset();
    m_Window.reset();
    m_EventBus.reset();

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

    if (m_PhysicsWorld) {
        m_PhysicsWorld->update(deltaTime);
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