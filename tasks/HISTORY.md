# Historique des Tâches (bb3d)

Ce fichier consigne l'historique compact de tous les chantiers et correctifs terminés.  
**Règle :** Format concis obligatoire (3 lignes max par tâche) pour minimiser la consommation de tokens.

---

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

