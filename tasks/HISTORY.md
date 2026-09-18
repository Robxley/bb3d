# Historique des Tâches (bb3d)

Ce fichier consigne l'historique compact de tous les chantiers et correctifs terminés.  
**Règle :** Format concis obligatoire (3 lignes max par tâche) pour minimiser la consommation de tokens.

---

- **[2026-09-18] [TASK-SYNC2-MIGRATION] Migration Complète vers Vulkan Synchronization2 (pipelineBarrier2 & DependencyInfo)** (@Antigravity / @bb3d-reviewer & @Robxley)
  - Scope: `src/bb3d/render/Renderer.cpp`, `src/bb3d/render/Texture.cpp`, `tests/unit_test_32_synchronization2.cpp`
  - Bilan: 18 barrières Vulkan 1.0 migrées vers `pipelineBarrier2` + `DependencyInfo`, batching couleur+profondeur, 0 pseudo-stage (eTopOfPipe/eBottomOfPipe), test 32 PASS (0.74s), 0 warning, revue APPROVED.
  - Archive: [archive/TASK-SYNC2-MIGRATION.md](archive/TASK-SYNC2-MIGRATION.md)

- **[2026-09-18] [FIX-COMPILATION-WARNINGS] Élimination Systématique des Warnings Compilateur MSVC** (@Antigravity / @Robxley)
  - Scope: `ImGuiLayer.cpp` (C4996 localtime_s), `GraphicsPipeline.cpp` (C4335 line endings), `Renderer.cpp` (C4100 currentFrame), `Texture.cpp` (C4834 nodiscard), `TextureGenerator.cpp` (C4189 variable c), `tests/unit_test_22/23.cpp`
  - Bilan: 100% zéro warning sur l'ensemble de la solution (biobazard3d, astro_bazard, éditeur, 31 tests), commentaires traduits en anglais, tests automatisés 100% PASS.

- **[2026-09-18] [TASK-VULKAN-1.4-INIT] Initialisation Vulkan 1.4 via StructureChain & Features Core** (@Antigravity / @bb3d-reviewer & @Robxley)
  - Scope: `include/bb3d/render/VulkanContext.hpp`, `src/bb3d/render/VulkanContext.cpp`, `tests/unit_test_02_vulkan_init.cpp`
  - Bilan: Vulkan 1.4/1.3 négocié, StructureChain type-safe avec query getFeatures2, core features (Sync2, DynamicRendering, TimelineSemaphore, PushDescriptor, Bindless), VMA aligné, tests 100% PASS, revue APPROVED.
  - Archive: [archive/TASK-VULKAN-1.4-INIT.md](archive/TASK-VULKAN-1.4-INIT.md)

- **[2026-09-17] [TASK-DEAD-CODE] Nettoyage du Code Mort Moteur (D3, D4, D5, ND1)** (@Antigravity / @bb3d-reviewer & @dev)
  - Scope: `include/bb3d/render/Renderer.hpp`, `src/bb3d/render/Renderer.cpp`, `src/bb3d/scene/Scene.cpp`, `include/bb3d/render/StagingBuffer.hpp`, `src/bb3d/render/StagingBuffer.cpp`
  - Bilan: D3 (m_instanceTransforms), D4 (getMaterialForTexture + m_defaultMaterials), D5 (horizon culling commenté), ND1 (StagingBuffer::submitCopy) supprimés sans régression, tests unitaires PASS, revue APPROVED.
  - Archive: [archive/TASK-DEAD-CODE.md](archive/TASK-DEAD-CODE.md)

- **[2026-09-17] [TASK-JOBSYSTEM] Architecture Hybride & Réveil Réactif du JobSystem (B24, B25)** (@Antigravity / @dev)
  - Scope: `include/bb3d/core/JobSystem.hpp`, `src/bb3d/core/JobSystem.cpp`, `tests/unit_test_08_core_systems.cpp`
  - Bilan: B24 (spin matériel _mm_pause + park OS, 0 busy-poll), B25 (callerIndex membre), wake latency 30 µs, 100% commentaires anglais, test 08 PASS, revue APPROVED.
  - Archive: [archive/TASK-JOBSYSTEM.md](archive/TASK-JOBSYSTEM.md)

- **[2026-09-17] [TASK-PHYSICS-GUARDS] Robustesse Jolt Physics (B14, B15, B16, B20, B21)** (@Antigravity / @bb3d-reviewer & @User)
  - Scope: `include/bb3d/physics/PhysicsWorld.hpp`, `src/bb3d/physics/PhysicsWorld.cpp`, `src/bb3d/scene/Scene.cpp`, `tests/unit_test_27_physics_guards.cpp`
  - Bilan: B14 (clamp threads), B15/B20 (gardes Transform), B16 (check CreateBody != null), B21 (nettoyage Jolt dans destroyEntity), test 27 PASS (0.48s), revue APPROVED.
  - Archive: [archive/TASK-PHYSICS-GUARDS.md](archive/TASK-PHYSICS-GUARDS.md)

- **[2026-09-17] [TASK-POSTPROCESS] Liaison PostProcessUBO & Synchronisation Viewport Éditeur (N1)** (@Antigravity / @User)
  - Scope: `src/bb3d/render/Renderer.cpp`, `include/bb3d/render/Renderer.hpp`, `include/bb3d/core/Config.hpp`, `src/bb3d/core/PickingSystem.cpp`, `CMakeLists.txt`
  - Bilan: PostProcessUBO lié au pipeline de copie (N1 résolu, 0 crash GPU), synchronisation picking/RenderTarget 1:1, timeouts CTest (15s) et fence (2s), tests PASS.
  - Archive: [archive/TASK-POSTPROCESS.md](archive/TASK-POSTPROCESS.md)

- **[2026-09-17] [TASK-B4-B5-B6] Robustesse & Performance GPU Color Picking** (@Antigravity / @bb3d-reviewer & @User)
  - Scope: `src/bb3d/render/Renderer.cpp`, `include/bb3d/render/Renderer.hpp`, `tests/unit_test_26_picking.cpp`
  - Bilan: B4 (fuite sets résolue), B5 (init/resize hors cb.begin, 0 waitIdle mid-frame), B6 (readback via fence isolée, 0 waitIdle global), Vulkan Sync2 pipelineBarrier2, test PASS, revue APPROVED.
  - Archive: [archive/TASK-B4-B5-B6-picking.md](archive/TASK-B4-B5-B6-picking.md)

- **[2026-09-17] [TASK-RENDER-CRITICAL] Sécurité Boucle de Rendu (B1, B2, B3)** (@bb3d-fixer / @bb3d-reviewer & @Antigravity)
  - Scope: `src/bb3d/render/Renderer.cpp`
  - Bilan: B1 (sémaphores swapchain), B2 (deadlock fence submit), B3 (lastMesh ombres) résolus, tests swapchain/shadows PASS, revue APPROVED.
  - Archive: [archive/TASK-RENDER-CRITICAL.md](archive/TASK-RENDER-CRITICAL.md)

- **[2026-09-17] [TASK-B8] Triple Buffering UBO Matériau & Synchronisation Frame** (@bb3d-fixer / @bb3d-reviewer & @Antigravity)
  - Scope: `src/bb3d/render/Renderer.cpp`, `include/bb3d/render/Material.hpp`, `tests/unit_test_25_material_frame.cpp`
  - Bilan: Double-check confirmé sur log Khronos, `Material::SetCurrentFrame` synchronisé par frame in flight, test unitaire validé, revue croisée conjointe APPROVED.
  - Archive: [archive/TASK-B8-material-ubo.md](archive/TASK-B8-material-ubo.md)

- **[2026-09-17] [SETUP-001] Audit Vulkan Moderne & Standardisation Tasks** (@agent / @user)
  - Scope: `docs/vulkan_audit/`, `.agents/skills/`, `tasks/`, `gemini.md`
  - Bilan: Audit Vulkan 1.3/1.4, skills `vulkan-cpp`/`cpp_pro`/`planification-revue` modernisés, setup `tasks/` et migration `docs/plans/`, tests: 29/29 PASS.
  - Archive: [docs/vulkan_audit/RAPPORT_AUDIT_VULKAN_MODERNE.md](../docs/vulkan_audit/RAPPORT_AUDIT_VULKAN_MODERNE.md)

- **[2026-03-21] [ARCHIVE] Sérialisation 2.0 & Component Registry** (@dev)
  - Scope: `src/bb3d/scene/`, `include/bb3d/scene/Components.hpp`
  - Bilan: Sauvegarde/chargement JSON de la hiérarchie et reconstruction des entités/composants, tests: PASS.
  - Archive: [archive/2026-03-21-component-registry-plan.md](archive/2026-03-21-component-registry-plan.md)

- **[2026-03-15] [ARCHIVE] Cascaded Shadow Mapping (CSM) & Planètes Procédurales** (@dev)
  - Scope: `src/bb3d/render/ShadowCascade.cpp`, `Renderer.cpp`, Shaders
  - Bilan: Implémentation CSM 4 cascades, calculs light-space stabilisés, génération maillage planètes, tests: PASS.
  - Archive: [archive/2026-03-15-shadow-system-design.md](archive/2026-03-15-shadow-system-design.md)

- **[2026-03-10] [ARCHIVE] AstroBazard Gameplay & Système de Particules** (@dev)
  - Scope: `apps/astro_bazard/`, `src/bb3d/render/ParticleSystem`
  - Bilan: Boucle de jeu spatiale 2D, intégration Jolt Physics, émetteur de particules et shader plasma, tests: PASS.
  - Archive: [archive/2026-03-10-astrobazard-plan.md](archive/2026-03-10-astrobazard-plan.md)

