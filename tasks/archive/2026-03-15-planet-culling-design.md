# Brainstorming: Planet Shading & Culling Optimization

## Problem Statement
The current procedural planet is rendered as a single large mesh. Even with hardware backface culling, the Vertex Shader processes all ~220k vertices, including those on the far side of the planet.

## Goals
1. Reduce vertex processing costs.
2. Leverage existing Frustum Culling.
3. Eliminate redundant calculation for faces hidden by the planet's own curvature.

## Proposed Approaches

### 1. Face Splitting
- **Concept**: Split the Cube Sphere into 6 separate Mesh entities.
- **Pros**: 
    - Automatic integration with existing `Frustum Culling`.
    - Simple to implement.
- **Cons**: 
    - 6 Draw calls instead of 1 (negligible for few planets).

### 2. Horizon Culling
- **Concept**: Use the camera's distance to the sphere to calculate a "horizon cone". Faces/Patches entirely behind the horizon are discarded.
- **Pros**: 
    - Most efficient for large planets.
- **Cons**: 
    - Requires custom math or CPU-side pruning per frame.

---
**Question for User**: Should we start with Face Splitting or go straight to Horizon Culling?
