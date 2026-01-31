#include "bb3d/render/Model.hpp"
#include "bb3d/core/Log.hpp"
#include "bb3d/resource/ResourceManager.hpp"
#include "bb3d/render/Texture.hpp"
#include "bb3d/render/Material.hpp"

#pragma warning(push, 0)
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#pragma warning(pop)

#include <iostream>
#include <array>
#include <variant>
#include <unordered_map>
#include <filesystem>

// Trait pour GLM dans fastgltf
template <>
struct fastgltf::ElementTraits<glm::vec3> : fastgltf::ElementTraitsBase<float, fastgltf::AccessorType::Vec3> {};

template <>
struct fastgltf::ElementTraits<glm::vec2> : fastgltf::ElementTraitsBase<float, fastgltf::AccessorType::Vec2> {};

template <>
struct fastgltf::ElementTraits<glm::vec4> : fastgltf::ElementTraitsBase<float, fastgltf::AccessorType::Vec4> {};

/** @brief Récupère les données d'un buffer fastgltf. */
static const std::byte* getBufferData(const fastgltf::Buffer& buffer) {
    return std::visit([](const auto& arg) -> const std::byte* {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, fastgltf::sources::Vector>) {
            return reinterpret_cast<const std::byte*>(arg.bytes.data());
        }
        else if constexpr (std::is_same_v<T, fastgltf::sources::ByteView>) {
            return reinterpret_cast<const std::byte*>(arg.bytes.data());
        }
        else if constexpr (std::is_same_v<T, fastgltf::sources::Array>) {
            return reinterpret_cast<const std::byte*>(arg.bytes.data());
        }
        else {
            return nullptr;
        }
    }, buffer.data);
}

namespace bb3d {

Model::~Model() {
    BB_CORE_TRACE("Model: Destroying model {} ({} meshes)", m_path, m_meshes.size());
}

Model::Model(VulkanContext& context, ResourceManager& resourceManager, std::string_view path)
    : Resource(std::string(path)), m_context(context), m_resourceManager(resourceManager) {
    
    std::filesystem::path p(path);
    if (p.extension() == ".obj") {
        loadOBJ(path);
    } else {
        loadGLTF(path);
    }
}

void Model::draw(vk::CommandBuffer commandBuffer) {
    for (const auto& mesh : m_meshes) {
        mesh->draw(commandBuffer);
    }
}

void Model::normalize(const glm::vec3& targetSize) {
    if (m_meshes.empty()) return;

    AABB globalBounds;
    for (const auto& mesh : m_meshes) globalBounds.extend(mesh->getBounds());

    glm::vec3 center = globalBounds.center();
    glm::vec3 size = globalBounds.size();
    
    float scaleX = (size.x > 0.0001f) ? targetSize.x / size.x : std::numeric_limits<float>::max();
    float scaleY = (size.y > 0.0001f) ? targetSize.y / size.y : std::numeric_limits<float>::max();
    float scaleZ = (size.z > 0.0001f) ? targetSize.z / size.z : std::numeric_limits<float>::max();
    float scale = std::min({scaleX, scaleY, scaleZ});
    if (scale == std::numeric_limits<float>::max()) scale = 1.0f;

    for (auto& mesh : m_meshes) {
        auto& vertices = mesh->getVertices();
        for (auto& v : vertices) v.position = (v.position - center) * scale;
        mesh->updateVertices();
    }

    m_bounds = AABB();
    for (const auto& mesh : m_meshes) m_bounds.extend(mesh->getBounds());
    BB_CORE_INFO("Model: Normalized (Scale: {}, Center Offset: {}, {}, {})", scale, center.x, center.y, center.z);
}

void Model::loadOBJ(std::string_view path) {
    BB_CORE_INFO("Model: Loading OBJ {}", path);

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    std::filesystem::path objPath(path);
    std::string baseDir = objPath.parent_path().string();
    if (!baseDir.empty()) baseDir += "/";

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.data(), baseDir.c_str())) {
        throw std::runtime_error("tinyobjloader error: " + err);
    }

    // 1. Charger les matériaux
    std::vector<Ref<Material>> modelMaterials;
    for (const auto& m : materials) {
        auto mat = CreateRef<UnlitMaterial>(m_context);
        
        if (!m.diffuse_texname.empty()) {
            std::string texPath = baseDir + m.diffuse_texname;
            try {
                mat->setBaseMap(m_resourceManager.load<Texture>(texPath, true));
            } catch (...) { BB_CORE_WARN("Model: Failed to load OBJ texture {}", texPath); }
        }
        
        mat->setColor(glm::vec3(m.diffuse[0], m.diffuse[1], m.diffuse[2]));
        modelMaterials.push_back(mat);
    }

    // 2. Traiter les géométries
    for (const auto& shape : shapes) {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::unordered_map<Vertex, uint32_t> uniqueVertices{};

        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};
            vertex.position = { attrib.vertices[3 * index.vertex_index + 0], attrib.vertices[3 * index.vertex_index + 1], attrib.vertices[3 * index.vertex_index + 2] };
            if (index.texcoord_index >= 0) vertex.uv = { attrib.texcoords[2 * index.texcoord_index + 0], attrib.texcoords[2 * index.texcoord_index + 1] };
            if (index.normal_index >= 0) vertex.normal = { attrib.normals[3 * index.normal_index + 0], attrib.normals[3 * index.normal_index + 1], attrib.normals[3 * index.normal_index + 2] };
            else vertex.normal = {0.0f, 1.0f, 0.0f};
            vertex.tangent = {1.0f, 0.0f, 0.0f, 1.0f};
            
            // Force vertex color to white for OBJ to avoid multiplication by black if colors are missing/zero
            vertex.color = {1.0f, 1.0f, 1.0f}; 

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }
            indices.push_back(uniqueVertices[vertex]);
        }

        auto mesh = CreateScope<Mesh>(m_context, vertices, indices);
        
        if (!shape.mesh.material_ids.empty() && shape.mesh.material_ids[0] >= 0) {
            size_t matId = static_cast<size_t>(shape.mesh.material_ids[0]);
            if (matId < modelMaterials.size()) {
                mesh->setMaterial(modelMaterials[matId]);
            }
        }

        m_bounds.extend(mesh->getBounds());
        m_meshes.push_back(std::move(mesh));
    }
}

void Model::loadGLTF(std::string_view path) {
    auto absPath = std::filesystem::absolute(path);
    BB_CORE_INFO("Model: Loading GLTF {}", absPath.string());

    fastgltf::Parser parser;
    constexpr auto options = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages;

    auto data = fastgltf::GltfDataBuffer::FromPath(absPath);
    if (data.error() != fastgltf::Error::None) throw std::runtime_error("GLTF: Failed to load file");

    auto type = fastgltf::determineGltfFileType(data.get());
    auto asset = (type == fastgltf::GltfType::GLB) ? parser.loadGltfBinary(data.get(), absPath.parent_path(), options) : parser.loadGltf(data.get(), absPath.parent_path(), options);
    if (asset.error() != fastgltf::Error::None) throw std::runtime_error("GLTF: Parse error");

    const auto& gltf = asset.get();

    for (const auto& image : gltf.images) {
        auto texture = std::visit(fastgltf::visitor {
            [&](const fastgltf::sources::Vector& vector) -> Ref<Texture> { return CreateRef<Texture>(m_context, std::span<const std::byte>(reinterpret_cast<const std::byte*>(vector.bytes.data()), vector.bytes.size())); },
            [&](const fastgltf::sources::ByteView& byteView) -> Ref<Texture> { return CreateRef<Texture>(m_context, std::span<const std::byte>(reinterpret_cast<const std::byte*>(byteView.bytes.data()), byteView.bytes.size())); },
            [&](const fastgltf::sources::BufferView& view) -> Ref<Texture> {
                auto& bv = gltf.bufferViews[view.bufferViewIndex];
                const std::byte* ptr = getBufferData(gltf.buffers[bv.bufferIndex]);
                return ptr ? CreateRef<Texture>(m_context, std::span<const std::byte>(ptr + bv.byteOffset, bv.byteLength)) : nullptr;
            },
            [](auto&) -> Ref<Texture> { return nullptr; }
        }, image.data);
        m_textures.push_back(texture);
    }

    for (const auto& mesh : gltf.meshes) {
        for (const auto& primitive : mesh.primitives) {
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;

            if (primitive.indicesAccessor.has_value()) {
                auto& accessor = gltf.accessors[primitive.indicesAccessor.value()];
                indices.resize(accessor.count);
                fastgltf::iterateAccessorWithIndex<uint32_t>(gltf, accessor, [&](uint32_t idx, size_t i) { indices[i] = idx; });
            }

            const auto* posAttr = primitive.findAttribute("POSITION");
            if (posAttr != primitive.attributes.end()) {
                auto& accessor = gltf.accessors[posAttr->accessorIndex];
                vertices.resize(accessor.count);
                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, accessor, [&](glm::vec3 pos, size_t i) {
                    vertices[i].position = pos; vertices[i].color = {1.0f, 1.0f, 1.0f}; vertices[i].normal = {0.0f, 1.0f, 0.0f}; vertices[i].tangent = {1.0f, 0.0f, 0.0f, 1.0f};
                });
            }

            const auto* normAttr = primitive.findAttribute("NORMAL");
            if (normAttr != primitive.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[normAttr->accessorIndex], [&](glm::vec3 norm, size_t i) { vertices[i].normal = norm; });
            }

            const auto* tanAttr = primitive.findAttribute("TANGENT");
            if (tanAttr != primitive.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[tanAttr->accessorIndex], [&](glm::vec4 tan, size_t i) { vertices[i].tangent = tan; });
            }

            const auto* uvAttr = primitive.findAttribute("TEXCOORD_0");
            if (uvAttr != primitive.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[uvAttr->accessorIndex], [&](glm::vec2 uv, size_t i) { vertices[i].uv = uv; });
            }

            auto newMesh = CreateScope<Mesh>(m_context, vertices, indices);
            
            if (primitive.materialIndex.has_value()) {
                const auto& material = gltf.materials[primitive.materialIndex.value()];
                if (material.pbrData.baseColorTexture.has_value()) {
                    size_t textureInfoIndex = material.pbrData.baseColorTexture.value().textureIndex;
                    if (textureInfoIndex < gltf.textures.size()) {
                        auto& texInfo = gltf.textures[textureInfoIndex];
                        if (texInfo.imageIndex.has_value()) {
                            size_t imageIndex = texInfo.imageIndex.value();
                            if (imageIndex < m_textures.size()) newMesh->setTexture(m_textures[imageIndex]);
                        }
                    }
                }
            }

            m_bounds.extend(newMesh->getBounds());
            m_meshes.push_back(std::move(newMesh));
        }
    }
}

} // namespace bb3d