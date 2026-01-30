#include "bb3d/render/Model.hpp"
#include "bb3d/core/Log.hpp"
#include "bb3d/resource/ResourceManager.hpp"
#include "bb3d/render/Texture.hpp"

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
        return (const std::byte*)nullptr;
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

    // 1. Calculer les bounds globaux actuels
    AABB globalBounds;
    for (const auto& mesh : m_meshes) {
        globalBounds.extend(mesh->getBounds());
    }

    glm::vec3 center = globalBounds.center();
    glm::vec3 size = globalBounds.size();
    
    // Calcul de l'échelle pour tenir dans targetSize (Uniform scaling)
    float scaleX = (size.x > 0.0001f) ? targetSize.x / size.x : std::numeric_limits<float>::max();
    float scaleY = (size.y > 0.0001f) ? targetSize.y / size.y : std::numeric_limits<float>::max();
    float scaleZ = (size.z > 0.0001f) ? targetSize.z / size.z : std::numeric_limits<float>::max();

    float scale = std::min({scaleX, scaleY, scaleZ});
    
    if (scale == std::numeric_limits<float>::max()) scale = 1.0f; // Fallback si taille nulle

    // 2. Appliquer la transformation à tous les sommets
    for (auto& mesh : m_meshes) {
        auto& vertices = mesh->getVertices();
        for (auto& v : vertices) {
            v.position = (v.position - center) * scale;
        }
        mesh->updateVertices();
    }

    // 3. Mettre à jour les bounds du modèle
    m_bounds = AABB();
    for (const auto& mesh : m_meshes) {
        m_bounds.extend(mesh->getBounds());
    }
    
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

    for (const auto& shape : shapes) {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::unordered_map<Vertex, uint32_t> uniqueVertices{};

        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};
            vertex.position = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            if (index.texcoord_index >= 0) {
                vertex.uv = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
            }

            if (!attrib.colors.empty()) {
                vertex.color = {
                    attrib.colors[3 * index.vertex_index + 0],
                    attrib.colors[3 * index.vertex_index + 1],
                    attrib.colors[3 * index.vertex_index + 2]
                };
            } else {
                vertex.color = {1.0f, 1.0f, 1.0f};
            }

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }
            indices.push_back(uniqueVertices[vertex]);
        }

        auto mesh = CreateScope<Mesh>(m_context, vertices, indices);
        m_bounds.extend(mesh->getBounds());
        m_meshes.push_back(std::move(mesh));
    }
}

void Model::loadGLTF(std::string_view path) {
    auto absPath = std::filesystem::absolute(path);
    BB_CORE_INFO("Model: Loading GLTF {}", absPath.string());

    fastgltf::Parser parser;
    constexpr auto options = fastgltf::Options::DontRequireValidAssetMember | 
                             fastgltf::Options::LoadExternalBuffers |
                             fastgltf::Options::LoadExternalImages;

    auto data = fastgltf::GltfDataBuffer::FromPath(absPath);
    if (data.error() != fastgltf::Error::None) throw std::runtime_error("GLTF: Failed to load file");

    auto type = fastgltf::determineGltfFileType(data.get());
    auto asset = (type == fastgltf::GltfType::GLB) 
        ? parser.loadGltfBinary(data.get(), absPath.parent_path(), options)
        : parser.loadGltf(data.get(), absPath.parent_path(), options);

    if (asset.error() != fastgltf::Error::None) throw std::runtime_error("GLTF: Parse error");

    const auto& gltf = asset.get();

    // Textures
    for (const auto& image : gltf.images) {
        auto texture = std::visit(fastgltf::visitor {
            [&](const fastgltf::sources::Vector& vector) -> Ref<Texture> {
                return CreateRef<Texture>(m_context, std::span<const std::byte>(reinterpret_cast<const std::byte*>(vector.bytes.data()), vector.bytes.size()));
            },
            [&](const fastgltf::sources::ByteView& byteView) -> Ref<Texture> {
                return CreateRef<Texture>(m_context, std::span<const std::byte>(reinterpret_cast<const std::byte*>(byteView.bytes.data()), byteView.bytes.size()));
            },
            [&](const fastgltf::sources::BufferView& view) -> Ref<Texture> {
                auto& bv = gltf.bufferViews[view.bufferViewIndex];
                const std::byte* ptr = getBufferData(gltf.buffers[bv.bufferIndex]);
                return ptr ? CreateRef<Texture>(m_context, std::span<const std::byte>(ptr + bv.byteOffset, bv.byteLength)) : nullptr;
            },
            [](auto&) -> Ref<Texture> { return nullptr; }
        }, image.data);
        m_textures.push_back(texture);
    }

    // Meshes
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
                    vertices[i].position = pos;
                    vertices[i].color = {1.0f, 1.0f, 1.0f};
                });
            }

            const auto* uvAttr = primitive.findAttribute("TEXCOORD_0");
            if (uvAttr != primitive.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[uvAttr->accessorIndex], [&](glm::vec2 uv, size_t i) {
                    vertices[i].uv = uv;
                });
            }

            auto newMesh = CreateScope<Mesh>(m_context, vertices, indices);
            
            // Texture Assignment
            if (primitive.materialIndex.has_value()) {
                const auto& material = gltf.materials[primitive.materialIndex.value()];
                if (material.pbrData.baseColorTexture.has_value()) {
                    size_t textureInfoIndex = material.pbrData.baseColorTexture.value().textureIndex;
                    // Note: fastgltf textureIndex points to a Texture object, which points to an Image index.
                    // But here m_textures stores images directly as fastgltf::Image ~ Texture.
                    // Wait, standard GLTF: Texture refers to Image + Sampler.
                    // fastgltf: gltf.textures[index].imageIndex
                    
                    if (textureInfoIndex < gltf.textures.size()) {
                        auto& texInfo = gltf.textures[textureInfoIndex];
                        if (texInfo.imageIndex.has_value()) {
                            size_t imageIndex = texInfo.imageIndex.value();
                            if (imageIndex < m_textures.size()) {
                                newMesh->setTexture(m_textures[imageIndex]);
                                BB_CORE_TRACE("Model: Assigned texture {} to mesh", imageIndex);
                            }
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
