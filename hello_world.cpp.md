/**
 * @file main.cpp
 * @brief "Kitchen Sink Demo" : Exemple exhaustif des fonctionnalités du moteur biobazard3d.
 * Démonstration de l'API Fluent, des systèmes (Audio, Physique, Jobs, Events) et de la sérialisation.
 */

#include "bb3d/Engine.hpp"
#include "bb3d/Scene.hpp" // Header unifié (Components, Entity)
#include "bb3d/core/Log.hpp"
#include "bb3d/core/JobSystem.hpp"
#include "bb3d/core/EventBus.hpp"
#include "bb3d/input/InputManager.hpp"

// Exemple d'événement personnalisé pour le Bus
struct PlayerScoreEvent { int points; };

int main(int argc, char* argv[]) {
    bb3d::Log::Init();

    // --- BLOC 1 : Initialisation & Création ---
    try {
        // 1. Initialisation Moteur (Configuration Builder)
        // Note: bb3d::Scope<Engine> est un std::unique_ptr
        auto engine = bb3d::Engine::Create(bb3d::EngineConfig()
            .resolution(1920, 1080)
            .vsync(true)
            .fpsMax(144)
            .title("Biobazard Ultimate Demo")
            // Activation explicite des modules (Zero Overhead par défaut - "Pay for what you use")
            .enablePhysics(bb3d::PhysicsBackend::Jolt)
            .enableAudio(true)
            .enableJobSystem(true)
        );

        // 2. Configuration des Systèmes Globaux
        
        // Input Mapping (Abstraction des touches pour permettre fallback SDL2 si SDL3 instable)
        engine->input().mapAction("Jump", bb3d::Key::Space);
        engine->input().mapAction("Fire", bb3d::Mouse::Left);
        engine->input().mapAxis("MoveY", bb3d::Key::W, bb3d::Key::S); // Avancer/Reculer

        // Event Bus (Abonnement)
        engine->events().subscribe<PlayerScoreEvent>([](const PlayerScoreEvent& e) {
            BB_CLIENT_INFO("Event reçu : Score joueur +{}", e.points);
        });

        // Job System (Lancement d'une tâche de fond)
        engine->jobs().execute([]() {
            BB_CLIENT_INFO("Tâche Background : Génération procédurale terminée.");
        });

        // 3. Création de la Scène
        // Note: Retourne bb3d::Ref<Scene> (std::shared_ptr) pour RAII strict
        auto scene = engine->createScene("DemoLevel");
        auto& assets = engine->assets();

        // --- Environnement (Skybox & Fog) ---
        // Note: Le système d'Assets utilise un cache interne. 
        // Charger deux fois la même texture renvoie le même pointeur intelligent.
        auto skyTexture = assets.load<bb3d::Texture>("env/sunset_hdr.ktx2");
        scene->setSkybox(skyTexture);
        scene->setFog({
            .color = {0.6f, 0.7f, 0.8f},
            .density = 0.015f,
            .type = bb3d::FogType::ExponentialHeight
        });

        // --- Assets ---
        auto heroMesh   = assets.load<bb3d::Model>("models/hero.glb");
        auto terrainMap = assets.load<bb3d::Texture>("maps/heightmap.png");
        auto fireSound  = assets.load<bb3d::AudioClip>("audio/fire_loop.mp3");
        auto smokeTex   = assets.load<bb3d::Texture>("fx/smoke_particle.png");
        auto toonShader = assets.load<bb3d::Shader>("shaders/toon.spv");

        // --- Entités (ECS) ---

        // A. Le Joueur (Physique + Audio Listener + Rendu Custom)
        scene->createEntity("Player")
            .at({0.0f, 2.0f, 0.0f}) // Définit Transform.Position
            .add<bb3d::Mesh>(heroMesh)
                .shader(toonShader) // Shader personnalisé
                .castShadows(true)
            // Note: Le RigidBody est "Maître" sur la position durant l'Update physique
            .add<bb3d::RigidBody>(bb3d::BodyType::Character, 80.0f) // Controleur Physique
            .add<bb3d::CapsuleCollider>(0.5f, 1.8f)
            .add<bb3d::AudioListener>() // "Les oreilles" du jeu (pour le son 3D)
            .add<bb3d::Script>("PlayerController"); // Logique (C++ ou Lua)

        // B. Caméra Orbitale (Suit le joueur)
        scene->createEntity("GameCamera")
            .add<bb3d::Camera>(bb3d::Projection::Perspective, 85.0f)
            .add<bb3d::OrbitCameraTarget>("Player")
            .makeActive();

        // C. Terrain (Génération depuis Heightmap)
        scene->createEntity("Terrain")
            .add<bb3d::Terrain>()
                .heightmap(terrainMap)
                .scale({1000.0f, 150.0f, 1000.0f})
                .lodFactor(2.5f); // Agressivité du LOD

        // D. Feu de Camp (Lumière + Son 3D + Particules)
        scene->createEntity("Campfire")
            .at({5.0f, 0.5f, 5.0f})
            .add<bb3d::Light>(bb3d::LightType::Point)
                .color({1.0f, 0.5f, 0.1f})
                .intensity(10.0f)
                .range(15.0f)
                .shadows(true) // Ombres dynamiques
            .add<bb3d::AudioSource>(fireSound)
                .loop(true)
                .volume(0.8f)
                .spatial(true) // Atténuation avec la distance
                .minMaxDistance(1.0f, 25.0f)
            .add<bb3d::ParticleSystem>()
                .texture(smokeTex)
                .rate(50) // part/sec
                .lifetime(1.5f, 3.0f)
                .velocity({0.0f, 1.5f, 0.0f}, 0.5f); // Vitesse vers le haut + variation

        // E. Objet Physique (Caisse)
        scene->createEntity("Crate")
            .at({2.0f, 10.0f, 2.0f})
            .add<bb3d::Mesh>(assets.load<bb3d::Model>("props/crate.glb"))
            .add<bb3d::RigidBody>(bb3d::BodyType::Dynamic, 10.0f)
            .add<bb3d::BoxCollider>(glm::vec3(1.0f));

        // --- Export & Run ---
        // Génère le fichier JSON complet décrivant l'état
        engine->exportScene("demo_state.json");
        BB_CLIENT_INFO("Scène initialisée et exportée.");
        
        engine->run();

    } catch (const std::exception& e) {
        BB_ERROR("Fatal Error (Init): {}", e.what());
        return -1;
    }

    // --- BLOC 2 : Rechargement (Preuve de Sérialisation) ---
    try {
        BB_CLIENT_INFO("Rechargement depuis la sauvegarde JSON...");
        auto engine = bb3d::Engine::Create(bb3d::EngineConfig().title("Biobazard Reloaded"));
        // L'import recrée toute la hiérarchie et réassigne les assets via le cache
        engine->importScene("demo_state.json");
        engine->run();
    } catch (const std::exception& e) {
        BB_ERROR("Fatal Error (Reload): {}", e.what());
        return -1;
    }

    return 0;
}
