# ☣️ biobazard3d (bb3d) - The Ultimate Engine Reference 

**biobazard3d** is a high-performance, modern 3D game engine and rendering framework written from scratch in **C++20** using the **Vulkan 1.3** API. Engineered to balance "zero-overhead" hardware access with high-level game-ready abstractions, it provides a comprehensive suite of features for rapid 3D application development.

---

## 🚀 Exhaustive Feature List

### 🎨 Rendering & Visuals
- **Vulkan 1.3 Backend**: Modern rendering using `Vulkan-Hpp` and dynamic rendering (no render passes/framebuffers maintenance).
- **Physically Based Rendering (PBR)**: Cook-Torrance BRDF workflow (Albedo, Normal, Metallic, Roughness, AO).
- **Dynamic Shadows (CSM)**: 4-cascade shadow mapping for directional lights with PCF filtering and adaptive bias.
- **Advanced Materials**: 
  - `PBRMaterial`: Full photorealistic pipeline.
  - `ToonMaterial`: Cell-shading with support for discrete shadow steps.
  - `UnlitMaterial`: Zero-lighting fast rendering.
  - `ParticleMaterial`: Translucency with additive blending.
- **Hardware Instancing**: Automatic SSBO-based batching for 10k+ dynamic objects.
- **Environment**: 4K HDR SkySpheres (Equirectangular) and Cubemaps with IBL support.
- **Post-Processing**: Offscreen rendering for editor viewports and custom resolution scaling.

### ⚙️ Core Architecture
- **ECS (Entity Component System)**: Powered by `EnTT`.
- **Custom Job System**: Work-stealing thread pool for async asset loading, culling, and physics.
- **Event Bus**: Decoupled messaging (Sync/Async) via the Publish/Subscribe pattern.
- **Resource Cache**: Deduplicated, thread-safe asynchronous loading for all assets.
- **Serialization**: Complete Save/Load system for scenes in human-readable JSON.

### 🌍 Simulation & Middleware
- **Jolt Physics**: Deterministic rigid body simulation (Sphere, Box, Capsule, Mesh Colliders).
- **Audio System**: Spatialized 3D audio (Miniaudio stub/backend).
- **Native Scripting**: C++ behaviors via lambda-based components.
- **Input System**: Multi-backend action mapping (Keyboard/Mouse).
- **Mouse Picking**: Dual-backend entity selection (Physics Raycast + GPU Color Picking) with per-entity selectability control.

---

## 💻 Technical Examples

### 1. Engine Initialization & Configuration
The engine is modular. Check the `Config.hpp` for all available toggles.

```cpp
#include <bb3d/core/Engine.hpp>

int main() {
    bb3d::EngineConfig config;
    config.title("My BB3D Game")
          .resolution(1920, 1080)
          .vsync(true)
          .enablePhysics(bb3d::PhysicsBackend::Jolt)
          .enableAudio(true)
          .enableEditor(true); // Boots the ImGui Inspector

    auto engine = bb3d::Engine::Create(config);
    auto scene = engine->CreateScene();
    engine->SetActiveScene(scene);

    engine->Run();
    return 0;
}
```

---

### 2. Cascaded Shadow Maps (CSM)
To enable shadows, you must set the flag on both the **Light** and the **Meshes** that should project them.

```cpp
// 1. Create Sun with Shadows enabled
auto sun = scene->createDirectionalLight("Sun", {1.0f, 1.0f, 0.9f}, 3.0f);
sun.get<bb3d::LightComponent>().castShadows = true;

// 2. Create a Mesh that casts shadows
auto meshEntity = scene->createEntity("Caster");
auto& mc = meshEntity.add<bb3d::MeshComponent>(myMesh);
mc.castShadows = true; // Required to be rendered in the shadow pass
```

---

### 3. Toon Rendering (Cell-Shading)
Use the `ToonMaterial` for stylized, anime-like visuals.

```cpp
auto toonMat = bb3d::CreateRef<bb3d::ToonMaterial>(engine->graphics());
toonMat->setBaseMap(engine->assets().load<bb3d::Texture>("character_diffuse.png"));
toonMat->setColor({1.0f, 0.5f, 0.5f}); // Tint

auto character = scene->createEntity("ToonChar");
character.add<bb3d::ModelComponent>(characterModel);
// Apply toon material to meshes
for(auto& m : characterModel->getMeshes()) m->setMaterial(toonMat);
```

---

### 4. Advanced Event Bus Communication
Communicate between isolated systems without pointers.

```cpp
struct PlayerJumpEvent { uint32_t entityID; float height; };

// Subscribe from anywhere (e.g., Audio system)
engine->events().subscribe<PlayerJumpEvent>([](const PlayerJumpEvent& e) {
    std::cout << "Player " << e.entityID << " jumped " << e.height << "m!\n";
});

// Publish synchronously
engine->events().publish(PlayerJumpEvent{ 42, 1.5f });

// Or Enqueue (processed at end of frame)
engine->events().enqueue(PlayerJumpEvent{ 42, 1.5f });
```

---

### 5. Multithreading with JobSystem
Distribute heavy work across all CPU cores.

```cpp
// A. Simple async execution
engine->jobs().execute([]() {
    // Perform heavy file IO or math here
});

// B. Parallel For (Dispatch)
// Splits 1,000,000 operations into batches of 1,000 for the thread pool
engine->jobs().dispatch(1000000, 1000, [](uint32_t start, uint32_t count) {
    for(uint32_t i = start; i < start + count; ++i) {
         // Do parallel work
    }
});
```

---

### 6. Built-in Mesh Highlighting
The `Renderer` provides built-in support for entity selection/hover highlighting.

```cpp
auto& renderer = engine->renderer();
auto& transform = myEntity.get<bb3d::TransformComponent>();
auto& mesh = myEntity.get<bb3d::MeshComponent>().mesh;

// Highlight the entity (e.g., in the editor)
renderer.setHighlightBounds(mesh->getBounds().transform(transform.getTransform()), true);

// Or Hover highlight (yellow outline)
renderer.setHoveredBounds(mesh->getBounds().transform(transform.getTransform()), true);
```

---

### 7. Procedural Planets & Biomes
Create worlds using multithreaded procedural generation.

```cpp
auto planet = scene->createEntity("Planet").add<bb3d::ProceduralPlanetComponent>();
planet.radius = 50.0f;
planet.resolution = 64;

bb3d::BiomeSettings ocean;
ocean.name = "Ocean"; ocean.color = {0, 0, 1}; ocean.heightEnd = 0.4f;
planet.biomes.push_back(ocean);

// Multithreaded generation
planet.model = bb3d::ProceduralMeshGenerator::createPlanet(
    engine->graphics(), engine->assets(), engine->jobs(), planet
);
```

---

### 14. Mouse Picking (Entity Selection)
The engine features a high-performance, dual-backend picking system integrated into the Editor viewport.

- **Backend 1: Physics Raycast** (Default)
  - Uses `Jolt Physics` for ultra-fast raycasting. 
  - Works on any entity with a `PhysicsComponent` (Box, Sphere, Mesh Colliders).
  - Falling back to **AABB Ray Intersection** (CPU-based) for non-physics entities.
- **Backend 2: GPU Color Picking**
  - Performs a dedicated rendering pass to an offscreen `R32_UINT` buffer.
  - Pixel-perfect accuracy: Each pixel stores the `entt::entity` handle.
  - Highly optimized: Uses dedicated instance buffers and a lightweight picking shader.
- **Selectability Control**: Opt-out selection via `bb3d::SelectableComponent`.

```cpp
// 1. Configuration (at startup)
bb3d::EngineConfig config;
config.enablePicking(bb3d::PickingMode::ColorPicking); 

// 2. Control Selectability (per entity)
auto ground = scene->createEntity("Ground");
ground.add<bb3d::SelectableComponent>({false}); // Immutable environment object

// 3. Scripted Picking (from mouse/UV)
glm::vec2 mouseUV = getMouseViewportUV();
bb3d::Entity hit = scene->pickEntity(mouseUV);
```

---

## 🏗 Resource Pathing
- `assets/models`: .gltf, .glb, .obj
- `assets/textures`: .png, .jpg, .hdr (Equirectangular)
- `assets/shaders`: .spv (SPIR-V binaries)

---

*biobazard3d architecture and codebase developed per standard documentation constraints. All namespaces isolate strictly underneath `bb3d::`.*