#include "bb3d/core/Engine.hpp"
#include "bb3d/core/Log.hpp"
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

    Log::Init();
    BB_CORE_INFO("Engine: Initializing biobazard3d...");

    m_Config = Config::Load(configPath);
    Init();
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

    // 3. Window
    m_Window = CreateScope<Window>(m_Config);

    // 4. Vulkan Context
    m_VulkanContext = CreateScope<VulkanContext>();
    m_VulkanContext->init(m_Window->GetNativeWindow(), m_Config.window.title, m_Config.graphics.enableValidationLayers);

    // 5. Resource Manager
    m_ResourceManager = CreateScope<ResourceManager>(*m_VulkanContext, *m_JobSystem);

    BB_CORE_INFO("Engine: Initialization complete.");
}

void Engine::Shutdown() {
    BB_PROFILE_SCOPE("Engine::Shutdown");
    BB_CORE_INFO("Engine: Shutting down...");

    if (m_VulkanContext && m_VulkanContext->getDevice()) {
        m_VulkanContext->getDevice().waitIdle();
    }

    m_ActiveScene.reset();
    m_ResourceManager.reset();
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

    uint64_t lastTime = SDL_GetTicks();

    while (m_Running && !m_Window->ShouldClose()) {
        BB_PROFILE_FRAME("MainLoop");

        uint64_t currentTime = SDL_GetTicks();
        float deltaTime = static_cast<float>(currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;

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
    return CreateRef<Scene>();
}

void Engine::Update(float deltaTime) {
    BB_PROFILE_SCOPE("Engine::Update");
    
    if (m_EventBus) {
        m_EventBus->dispatchQueued();
    }

    // Ici sera inséré la logique de mise à jour système (Physique -> Scripting -> Anim)
}

void Engine::Render() {
    BB_PROFILE_SCOPE("Engine::Render");
    
    // Ici sera inséré l'appel au Renderer avec la scène active
}

} // namespace bb3d