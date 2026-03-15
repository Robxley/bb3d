#pragma once

#include "bb3d/render/Mesh.hpp"
#include "bb3d/scene/Components.hpp"
#include <glm/glm.hpp>

namespace bb3d {
class JobSystem;

/**
 * @brief Utility for generating complex procedural meshes such as planets.
 */
class ProceduralMeshGenerator {
public:
    /**
     * @brief Generates a procedural planet mesh based on Cube Sphere topology.
     * @param context Vulkan context.
     * @param component The procedural planet configuration.
     * @return Scope<Mesh> The generated mesh.
     */
    static Scope<Model> createPlanet(VulkanContext& context, ResourceManager& resourceManager, JobSystem& jobSystem, const ProceduralPlanetComponent& component);

private:
    struct FaceData {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
    };

    static FaceData generateFaceData(
        const glm::vec3& localUp, 
        const ProceduralPlanetComponent& component
    );
};

} // namespace bb3d
