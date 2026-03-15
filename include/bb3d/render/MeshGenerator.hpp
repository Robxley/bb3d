#pragma once

#include "bb3d/render/Mesh.hpp"
#include <glm/glm.hpp>
#include <vector>

namespace bb3d {

/**
 * @brief Utility for generating simple primitive shape meshes.
 */
class MeshGenerator {
public:
    /**
     * @brief Generates a cube centered at the origin.
     * @param context Vulkan context for buffer creation.
     * @param size Cube size (side length).
     * @param color Color applied to all vertices.
     * @return Scope<Mesh> The generated mesh.
     */
    static Scope<Mesh> createCube(VulkanContext& context, float size = 1.0f, const glm::vec3& color = {1.0f, 1.0f, 1.0f});

    /**
     * @brief Generates a wireframe cube centered at the origin, with 12 edge lines.
     * @param context Vulkan context.
     * @param size Cube size (side length).
     * @param color Color applied to all vertices.
     * @return Scope<Mesh> The generated mesh for LINE_LIST rendering.
     */
    static Scope<Mesh> createWireframeCube(VulkanContext& context, float size = 1.0f, const glm::vec3& color = {1.0f, 1.0f, 1.0f});

    /**
     * @brief Generates a UV sphere.
     * @param context Vulkan context.
     * @param radius Sphere radius.
     * @param segments Number of horizontal and vertical segments.
     * @param color Base color.
     * @return Scope<Mesh> The generated mesh.
     */
    static Scope<Mesh> createSphere(VulkanContext& context, float radius = 0.5f, uint32_t segments = 32, const glm::vec3& color = {1.0f, 1.0f, 1.0f});

    /**
     * @brief Generates a cone.
     * @param context Vulkan context.
     * @param radius Base radius.
     * @param height Cone height.
     * @param segments Number of radial segments.
     * @param color Base color.
     * @return Scope<Mesh> The generated mesh.
     */
    static Scope<Mesh> createCone(VulkanContext& context, float radius = 0.5f, float height = 1.0f, uint32_t segments = 16, const glm::vec3& color = {1.0f, 1.0f, 1.0f});

    /**
     * @brief Generates a horizontal plane (XZ) with a colored checkerboard pattern.
     * @param context Vulkan context.
     * @param size Total plane size.
     * @param subdivisions Number of squares per side.
     * @param color1 First checkerboard color.
     * @param color2 Second checkerboard color.
     * @return Scope<Mesh> The generated mesh.
     */
    static Scope<Mesh> createCheckerboardPlane(VulkanContext& context, float size = 10.0f, uint32_t subdivisions = 10, const glm::vec3& color1 = {0.2f, 0.2f, 0.2f}, const glm::vec3& color2 = {0.8f, 0.8f, 0.8f});

    /**
     * @brief Generates a simple 2D quad (rectangle) on the XY plane.
     * @param context Vulkan context.
     * @param size Quad size (side length).
     * @param color Base color.
     * @return Scope<Mesh> The generated mesh.
     */
    static Scope<Mesh> createQuad(VulkanContext& context, float size = 1.0f, const glm::vec3& color = {1.0f, 1.0f, 1.0f});
};

} // namespace bb3d
