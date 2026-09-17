# Historique des Tâches (bb3d)

Ce fichier consigne l'historique compact de tous les chantiers et correctifs terminés.  
**Règle :** Format concis obligatoire (3 lignes max par tâche) pour minimiser la consommation de tokens.

---

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

