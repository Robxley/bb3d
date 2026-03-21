#include "bb3d/core/Engine.hpp"
#include "bb3d/core/Log.hpp"
#include "bb3d/scene/Components.hpp"
#include "bb3d/physics/PhysicsWorld.hpp"
#include "bb3d/audio/AudioSystem.hpp"
#include "bb3d/scene/SceneSerializer.hpp"
#include "bb3d/render/Material.hpp"
#include "bb3d/core/PickingSystem.hpp"
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

Scope<Engine> Engine::Create(const std::string_view configPath) {
    return CreateScope<Engine>(configPath);
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

    BB_CORE_INFO("Engine: Module Status:");
    BB_CORE_INFO(" - JobSystem: {}", m_Config.modules.enableJobSystem ? "ENABLED" : "DISABLED");
    BB_CORE_INFO(" - Physics:   {}", m_Config.modules.enablePhysics ? "ENABLED" : "DISABLED");
    BB_CORE_INFO(" - Audio:     {}", m_Config.modules.enableAudio ? "ENABLED" : "DISABLED");
    BB_CORE_INFO(" - Editor:    {}", m_Config.modules.enableEditor ? "ENABLED" : "DISABLED");

    // 1. Job System (Initialized first to be available for async loads)
    if (m_Config.modules.enableJobSystem) {
        m_JobSystem = CreateScope<JobSystem>();
        m_JobSystem->init(m_Config.system.maxThreads);
    } else {
        BB_CORE_WARN("Engine: JobSystem is disabled in config.");
    }

    // 2. Event Bus (Decoupled communication between systems)
    m_EventBus = CreateScope<EventBus>();

    // 3. Input Manager (Must be ready before window creation for callbacks)
    m_InputManager = CreateScope<InputManager>();

    // 4. Window (SDL)
    // Necessary to create the Vulkan surface later.
    m_Window = CreateScope<Window>(m_Config);
    m_Window->SetEventCallback([this](SDL_Event& e) {
#if defined(BB3D_ENABLE_EDITOR)
        if (m_ImGuiLayer) m_ImGuiLayer->onEvent(e);
#endif
        if (m_InputManager) m_InputManager->onEvent(e);
        
        // Resize handling: Notify renderer to rebuild SwapChain
        if (e.type == SDL_EVENT_WINDOW_RESIZED || e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
            int w = e.window.data1;
            int h = e.window.data2;
            if (w > 0 && h > 0 && m_Renderer) {
                m_Renderer->onResize(w, h);
            } else if (w == 0 || h == 0) {
                BB_CORE_WARN("Engine::HandleEvent: Window resized to invalid dimensions ({}x{}). Ignoring.", w, h);
            }
        }
    });

    // 5. Vulkan Context (Instance, PhysicalDevice, LogicalDevice)
    // Initializes Vulkan by attaching to the SDL window created just before.
    m_VulkanContext = CreateScope<VulkanContext>();
    m_VulkanContext->init(m_Window->GetNativeWindow(), m_Config.window.title, m_Config.graphics.enableValidationLayers);

    // 6. Renderer (SwapChain, Pipelines, RenderPasses)
    // Depends on VulkanContext and Window.
    m_Renderer = CreateScope<Renderer>(*m_VulkanContext, *m_Window, *m_JobSystem, m_Config);

    // 6.5 ImGui Layer (Editor UI)
#if defined(BB3D_ENABLE_EDITOR)
    if (m_Config.modules.enableEditor) {
        m_ImGuiLayer = CreateScope<ImGuiLayer>(*m_VulkanContext, *m_Window, m_Renderer->getSwapChain());
    }
#endif

    // 7. Resource Manager (Textures, Models, Shaders)
    // Manages loading and caching. Uses JobSystem for async loading.
    m_ResourceManager = CreateScope<ResourceManager>(*m_VulkanContext, *m_JobSystem);

    // 8. Physics (Jolt)
    // Independent of rendering, can be initialized here.
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

    // 10. Picking System
    m_PickingSystem = CreateScope<PickingSystem>(m_Config.modules.pickingMode);
}

void Engine::Shutdown() {
    BB_PROFILE_SCOPE("Engine::Shutdown");
    BB_CORE_INFO("Engine: Shutting down...");

    // Wait for the GPU to finish its work before destroying everything
    if (m_VulkanContext && m_VulkanContext->getDevice()) {
        try {
            m_VulkanContext->getDevice().waitIdle();
        } catch(...) {}
    }

    // 1. Picking
    m_PickingSystem.reset();

    // 2. Audio
    if (m_AudioSystem) {
        m_AudioSystem->shutdown();
        m_AudioSystem.reset();
    }

    // 2. Physics
    if (m_PhysicsWorld) {
        m_PhysicsWorld->shutdown();
        m_PhysicsWorld.reset();
    }

    // 3. Active Scene (Destroy Entities, Components, Scripts)
    if (m_ActiveScene) {
        m_ActiveScene->getRegistry().clear();
    }
    m_ActiveScene.reset();

#if defined(BB3D_ENABLE_EDITOR)
    m_ImGuiLayer.reset();
#endif
    
    // 4. Renderer
    m_Renderer.reset(); 

    // 5. Job System (Stop threads BEFORE clearing resources they might be using)
    if (m_JobSystem) {
        m_JobSystem->shutdown();
        m_JobSystem.reset();
    }

    // 6. Resources (Texture/Model cache)
    if (m_ResourceManager) {
        m_ResourceManager->clearCache();
        m_ResourceManager.reset();
    }

    // Release Material static resources (e.g., white sampler)
    // Must be done after all materials/textures are cleared.
    Material::Cleanup();

    // 7. Graphics Context (Contains VMA allocator)
    if (m_VulkanContext && m_VulkanContext->getDevice()) {
        try { m_VulkanContext->getDevice().waitIdle(); } catch(...) {}
    }
    m_VulkanContext.reset();

    // 8. Window
    m_Window.reset();

    // 9. Communication & Input
    m_EventBus.reset();
    m_InputManager.reset();

    BB_CORE_INFO("Engine: Shutdown complete.");
}


void Engine::Run() {
    m_Running = true;
    BB_CORE_INFO("Engine: Entering main loop.");

    uint64_t lastTime = SDL_GetTicks();

    while (m_Running && !m_Window->ShouldClose()) {
        BB_PROFILE_FRAME("MainLoop");

        // 0. Reset input deltas for the new frame
        if (m_InputManager) {
            m_InputManager->clearDeltas();
        }

        uint64_t currentTime = SDL_GetTicks();
        float deltaTime = static_cast<float>(currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;

        // Limit deltaTime to avoid physics explosions during startup or after a freeze
        if (deltaTime > 0.1f) deltaTime = 0.1f;

        // 1. System Events (Window, Input)
        m_Window->PollEvents();
        
        // If the window is minimized (size 0), skip rendering and logic to avoid CPU usage
        // and Vulkan/ImGui errors.
        if (m_Window->GetWidth() == 0 || m_Window->GetHeight() == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
#if defined(BB3D_ENABLE_EDITOR)
        if (m_Config.modules.enableEditor && m_ImGuiLayer) m_ImGuiLayer->beginFrame();
#endif

        // Update input state (reset deltas)
        if (m_InputManager) {
            // Block game inputs if ImGui captures mouse/keyboard
            bool captureMouse = false;
            bool captureKeyboard = false;
#if defined(BB3D_ENABLE_EDITOR)
            if (m_Config.modules.enableEditor && m_ImGuiLayer) {
                // On autorise le jeu à recevoir les inputs si :
                // 1. La souris survole le Viewport
                // 2. OU si le Viewport est focus et qu'on est en train de cliquer/drag
                bool isInteracting = m_ImGuiLayer->isViewportHovered() || (m_ImGuiLayer->isViewportFocused() && ImGui::IsAnyMouseDown());
                captureMouse = m_ImGuiLayer->wantCaptureMouse() && !isInteracting;
                
                // Le clavier est libéré dès que le Viewport est focus
                captureKeyboard = m_ImGuiLayer->wantCaptureKeyboard() && !m_ImGuiLayer->isViewportFocused();
            }
#endif
            m_InputManager->update(captureMouse, captureKeyboard);
        }

        // 2. Business Logic Update
        Update(deltaTime);

        // 3. Render
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
    
    // Dispatch queued events
    if (m_EventBus) {
        m_EventBus->dispatchQueued();
    }

    if (m_ActiveScene) {
        // 1. Physics (Master truth on Transform)
        if (m_PhysicsWorld && m_Config.modules.enablePhysics && !m_PhysicsPaused) {
            m_PhysicsWorld->update(deltaTime, *m_ActiveScene);
        }

        // 2. Cameras & Logic
        m_ActiveScene->onUpdate(deltaTime);
    }
}

void Engine::resetScene() {
    if (!m_ActiveScene) return;

    BB_CORE_INFO("Engine: Resetting scene...");

    // 1. Restore all transforms to their initial state
    auto transformView = m_ActiveScene->getRegistry().view<TransformComponent>();
    for (auto entity : transformView) {
        transformView.get<TransformComponent>(entity).resetToInitial();
    }

    // 2. Reset physical bodies (velocity to zero, teleportation)
    if (m_PhysicsWorld) {
        m_PhysicsWorld->resetAllBodies(*m_ActiveScene);
    }
}
void Engine::Render() {
    BB_PROFILE_SCOPE("Engine::Render");

    if (!m_Renderer) return;

#if defined(BB3D_ENABLE_EDITOR)
    // 0. Handle deferred viewport resize requested by ImGui in previous frame
    if (m_Config.modules.enableEditor && m_ImGuiLayer && m_ImGuiLayer->hasViewportSizeChanged()) {
        glm::uvec2 size = m_ImGuiLayer->getViewportSize();
        if (size.x > 0 && size.y > 0) {
            auto rt = m_Renderer->getRenderTarget();
            if (rt) {
                rt->resize(size.x, size.y);

                // Update camera aspect ratio
                if (m_ActiveScene) {
                    auto view = m_ActiveScene->getRegistry().view<CameraComponent>();
                    for (auto entity : view) {
                        auto& cc = view.get<CameraComponent>(entity);
                        if (cc.active && cc.camera) cc.camera->setAspectRatio((float)size.x / (float)size.y);
                    }
                }
                BB_CORE_TRACE("Engine: Viewport RenderTarget resized to {}x{}", size.x, size.y);
            }
        }
        m_ImGuiLayer->clearViewportSizeChanged();
    }
#endif

    bool frameStarted = false;
    if (m_Renderer && m_ActiveScene) {
#if defined(BB3D_ENABLE_EDITOR)
        // Update highlight bounds
        if (m_Config.modules.enableEditor && m_ImGuiLayer) {
            auto getEntityBounds = [](Entity e) -> AABB {
                glm::mat4 tform = e.has<TransformComponent>() ? e.get<TransformComponent>().getTransform() : glm::mat4(1.0f);
                if (e.has<MeshComponent>() && e.get<MeshComponent>().mesh) {
                    return e.get<MeshComponent>().mesh->getBounds().transform(tform);
                } else if (e.has<ModelComponent>() && e.get<ModelComponent>().model) {
                    return e.get<ModelComponent>().model->getBounds().transform(tform);
                } else if (e.has<PhysicsComponent>()) {
                    auto& phys = e.get<PhysicsComponent>();
                    if (phys.colliderType == ColliderType::Box) return AABB(-phys.boxHalfExtents, phys.boxHalfExtents).transform(tform);
                    if (phys.colliderType == ColliderType::Sphere) return AABB(glm::vec3(-phys.radius), glm::vec3(phys.radius)).transform(tform);
                    return AABB(glm::vec3(-0.5f), glm::vec3(0.5f)).transform(tform);
                } else {
                    return AABB(glm::vec3(-0.5f), glm::vec3(0.5f)).transform(tform); 
                }
            };

            Entity selected = m_ImGuiLayer->getSelectedEntity();
            if (selected) {
                m_Renderer->setHighlightBounds(getEntityBounds(selected), true);
            } else {
                m_Renderer->setHighlightBounds({}, false);
            }

            Entity hovered = m_ImGuiLayer->getHoveredEntity();
            if (hovered && hovered != selected) {
                m_Renderer->setHoveredBounds(getEntityBounds(hovered), true);
            } else {
                m_Renderer->setHoveredBounds({}, false);
            }
        }
#endif

        // 1. Scene Rendering
        frameStarted = m_Renderer->render(*m_ActiveScene);

        // 1b. GPU Color Picking pass (renders entity IDs to a separate buffer)
        if (frameStarted && m_PickingSystem && m_PickingSystem->getMode() == PickingMode::ColorPicking) {
            m_Renderer->renderEntityIds(*m_ActiveScene);
        }
    }
#if defined(BB3D_ENABLE_EDITOR)
    // 2. UI Rendering (ImGui) overlay
    if (m_Config.modules.enableEditor && m_ImGuiLayer) {
        m_ImGuiLayer->beginDockspace();

        m_ImGuiLayer->showViewport(m_Renderer->getRenderTarget(), *m_ActiveScene);
        m_ImGuiLayer->showMainMenu();
        m_ImGuiLayer->showSceneHierarchy(*m_ActiveScene);
        m_ImGuiLayer->showSceneSettings(*m_ActiveScene);
        m_ImGuiLayer->showInspector();
        m_ImGuiLayer->showToolbar();

        m_ImGuiLayer->endDockspace();

        if (frameStarted && m_Renderer) {
            // Command buffer available, draw ImGui normally
            m_Renderer->renderUI([this](vk::CommandBuffer cb) {
                m_ImGuiLayer->endFrame(cb);
            });
        } else {
            // No rendering this time (or no renderer/scene),
            // just finish ImGui frame for internal state
            m_ImGuiLayer->endFrame();
        }
    }
#endif

    if (frameStarted && m_Renderer) {
        // 3. Submission and Presentation (SEND TO GPU)
        m_Renderer->submitAndPresent();
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
    
    // Pause physics by default during loading
    m_PhysicsPaused = true;

    SceneSerializer serializer(scene);
    if (serializer.deserialize(filepath)) {
        m_ActiveScene = scene;
#if defined(BB3D_ENABLE_EDITOR)
        if (m_ImGuiLayer) {
            m_ImGuiLayer->setSelectedEntity({});
        }
#endif
        // Auto-reset to apply initial states and synchronize physics
        resetScene();
        
        BB_CORE_INFO("Engine: Scene loaded successfully and physics PAUSED.");
    } else {
        BB_CORE_ERROR("Engine: Failed to load scene from {}", filepath);
    }
}

} // namespace bb3d
