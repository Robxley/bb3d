#include "bb3d/resource/Resource.hpp"
#include "bb3d/render/Mesh.hpp"
#include "bb3d/render/VulkanContext.hpp"
#include <vector>
#include <string_view>

namespace bb3d {

class ResourceManager; // Forward declaration

class Model : public Resource {
public:
    Model(VulkanContext& context, ResourceManager& resourceManager, std::string_view path);
    ~Model() override = default;

    void draw(VkCommandBuffer commandBuffer);
    const AABB& getBounds() const { return m_bounds; }

private:
    void loadGLTF(std::string_view path);
    void loadOBJ(std::string_view path);

    VulkanContext& m_context;
    ResourceManager& m_resourceManager;
    std::vector<Scope<Mesh>> m_meshes;
    AABB m_bounds;
};

} // namespace bb3d