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

// Define traits for GLM types to be used with fastgltf
template <>
struct fastgltf::ElementTraits<glm::vec3> : fastgltf::ElementTraitsBase<float, fastgltf::AccessorType::Vec3> {};

template <>
struct fastgltf::ElementTraits<glm::vec2> : fastgltf::ElementTraitsBase<float, fastgltf::AccessorType::Vec2> {};

// Helper pour récupérer le pointeur de données depuis le variant fastgltf::DataSource
const uint8_t* getBufferData(const fastgltf::Buffer& buffer) {
    return std::visit([](const auto& arg) -> const uint8_t* {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, fastgltf::sources::Vector>) {
            return reinterpret_cast<const uint8_t*>(arg.bytes.data());
        }
        else if constexpr (std::is_same_v<T, fastgltf::sources::ByteView>) {
            return reinterpret_cast<const uint8_t*>(arg.bytes.data());
        }
        else if constexpr (std::is_same_v<T, fastgltf::sources::Array>) {
            return reinterpret_cast<const uint8_t*>(arg.bytes.data());
        }
        else if constexpr (std::is_same_v<T, std::monostate>) {
            return nullptr;
        }
        else {
            return nullptr;
        }
    }, buffer.data);
}

namespace bb3d {

Model::Model(VulkanContext& context, ResourceManager& resourceManager, std::string_view path)
    : Resource(std::string(path)), m_context(context), m_resourceManager(resourceManager) {
    
    std::filesystem::path p(path);
    if (p.extension() == ".obj") {
        loadOBJ(path);
    } else {
        loadGLTF(path);
    }
}

void Model::draw(VkCommandBuffer commandBuffer) {
    for (const auto& mesh : m_meshes) {
        mesh->draw(commandBuffer);
    }
}

void Model::loadOBJ(std::string_view path) {
    BB_CORE_INFO("Chargement du modèle OBJ: {}", path);

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    std::filesystem::path objPath(path);
    std::string baseDir = objPath.parent_path().string();
    if (!baseDir.empty()) baseDir += "/";

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.data(), baseDir.c_str())) {
        throw std::runtime_error("Erreur tinyobjloader: " + warn + err);
    }

    if (!warn.empty()) {
        BB_CORE_WARN("tinyobjloader warning: {}", warn);
    }

    // Pour chaque shape (mesh)
    for (const auto& shape : shapes) {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::unordered_map<Vertex, uint32_t> uniqueVertices{};

        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            // Position
            vertex.position = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            // UV
            if (index.texcoord_index >= 0) {
                vertex.uv = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1] // Flip V for Vulkan
                };
            }

            // Color
            if (!attrib.colors.empty()) {
                vertex.color = {
                    attrib.colors[3 * index.vertex_index + 0],
                    attrib.colors[3 * index.vertex_index + 1],
                    attrib.colors[3 * index.vertex_index + 2]
                };
            }
            // Fallback: Normal -> Color (pour debug visuel simple)
            else if (index.normal_index >= 0) {
                // Map normals [-1, 1] to color [0, 1]
                vertex.color = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                };
                // Optional: normalize to 0..1 for visualization
                vertex.color = vertex.color * 0.5f + 0.5f; 
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
        
        // Texture loading (Cache warmup)
        if (!shape.mesh.material_ids.empty() && shape.mesh.material_ids[0] >= 0) {
            int matId = shape.mesh.material_ids[0];
            if (!materials[matId].diffuse_texname.empty()) {
                std::string texPath = baseDir + materials[matId].diffuse_texname;
                // Just load to ensure it works and is cached
                try {
                    m_resourceManager.load<Texture>(texPath); 
                    BB_CORE_INFO("Texture chargée pour OBJ: {}", texPath);
                } catch (const std::exception& e) {
                    BB_CORE_ERROR("Impossible de charger la texture OBJ: {} ({})", texPath, e.what());
                }
            }
        }
    }
}

void Model::loadGLTF(std::string_view path) {
    auto absPath = std::filesystem::absolute(path);
    BB_CORE_INFO("Chargement du modèle GLTF: {} (Chemin absolu: {})", path, absPath.string());

    fastgltf::Parser parser;
    constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | 
                                 fastgltf::Options::AllowDouble | 
                                 fastgltf::Options::LoadExternalBuffers |
                                 fastgltf::Options::LoadExternalImages;

    auto data = fastgltf::GltfDataBuffer::FromPath(absPath);
    if (data.error() != fastgltf::Error::None) {
        throw std::runtime_error("Impossible de charger le fichier GLTF (FromPath): " + absPath.string() + " Error: " + std::to_string(fastgltf::to_underlying(data.error())));
    }

    auto type = fastgltf::determineGltfFileType(data.get());
    fastgltf::Expected<fastgltf::Asset> asset = fastgltf::Error::None;

    if (type == fastgltf::GltfType::GLB) {
        asset = parser.loadGltfBinary(data.get(), absPath.parent_path(), gltfOptions);
    } else if (type == fastgltf::GltfType::glTF) {
        asset = parser.loadGltf(data.get(), absPath.parent_path(), gltfOptions);
    } else {
        throw std::runtime_error("Format GLTF non reconnu pour: " + absPath.string());
    }

    if (asset.error() != fastgltf::Error::None) {
        std::string errorMsg = "Erreur de parsing GLTF (" + std::string(absPath.extension().string()) + "): " + 
                               std::string(fastgltf::getErrorName(asset.error()));
        BB_CORE_ERROR(errorMsg);
        throw std::runtime_error(errorMsg);
    }

    const auto& gltf = asset.get();

    // Iterate over meshes
    for (const auto& mesh : gltf.meshes) {
        BB_CORE_INFO("Parsing Mesh: {}", mesh.name);

        for (const auto& primitive : mesh.primitives) {
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;

            // --- Indices ---
            if (primitive.indicesAccessor.has_value()) {
                auto& accessor = gltf.accessors[primitive.indicesAccessor.value()];
                indices.resize(accessor.count);
                
                fastgltf::iterateAccessorWithIndex<uint32_t>(gltf, accessor, [&](uint32_t index, size_t i) {
                    indices[i] = index;
                });
            }

            // --- Attributes ---
            const auto* positionIt = primitive.findAttribute("POSITION");
            const auto* uvIt = primitive.findAttribute("TEXCOORD_0");

            size_t vertexCount = 0;
            if (positionIt != primitive.attributes.end()) {
                vertexCount = gltf.accessors[positionIt->accessorIndex].count;
            }
            vertices.resize(vertexCount);

            // Position
            if (positionIt != primitive.attributes.end()) {
                auto& accessor = gltf.accessors[positionIt->accessorIndex];
                
                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, accessor, [&](glm::vec3 pos, size_t i) {
                    vertices[i].position = pos;
                    vertices[i].color = {1.0f, 1.0f, 1.0f};
                });
            }

            // UV
            if (uvIt != primitive.attributes.end()) {
                auto& accessor = gltf.accessors[uvIt->accessorIndex];
                
                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, accessor, [&](glm::vec2 uv, size_t i) {
                    vertices[i].uv = uv;
                });
            }

            auto newMesh = CreateScope<Mesh>(m_context, vertices, indices);
            m_bounds.extend(newMesh->getBounds());
            m_meshes.push_back(std::move(newMesh));
        }
    }

    BB_CORE_INFO("Modèle chargé avec succès. {} meshes créés.", m_meshes.size());
}

} // namespace bb3d