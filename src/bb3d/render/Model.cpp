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

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <iostream>
#include <array>
#include <variant>
#include <unordered_map>
#include <filesystem>

// Trait for GLM in fastgltf
template <>
struct fastgltf::ElementTraits<glm::vec3> : fastgltf::ElementTraitsBase<float, fastgltf::AccessorType::Vec3> {};

template <>
struct fastgltf::ElementTraits<glm::vec2> : fastgltf::ElementTraitsBase<float, fastgltf::AccessorType::Vec2> {};

template <>
struct fastgltf::ElementTraits<glm::vec4> : fastgltf::ElementTraitsBase<float, fastgltf::AccessorType::Vec4> {};

/** @brief Retrieves data from a fastgltf buffer. */
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

Model::Model(VulkanContext& context, ResourceManager& resourceManager, std::string_view path, const ModelLoadConfig& config)
    : Resource(std::string(path)), m_context(context), m_resourceManager(resourceManager) {
    
    std::filesystem::path p(path);
    if (p.extension() == ".obj") {
        loadOBJ(path, config);
    } else {
        loadGLTF(path, config);
    }
}

Model::Model(VulkanContext& context, ResourceManager& resourceManager)
    : Resource("ProceduralModel"), m_context(context), m_resourceManager(resourceManager) {
}

void Model::draw(vk::CommandBuffer commandBuffer) {
    for (const auto& mesh : m_meshes) {
        mesh->draw(commandBuffer);
    }
}

glm::vec3 Model::normalize(const glm::vec3& targetSize) {
    if (m_meshes.empty()) return glm::vec3(0.0f);

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
    
    return center;
}

void Model::loadOBJ(std::string_view path, const ModelLoadConfig& config) {
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

    // 1. Load materials
    std::vector<Ref<Material>> modelMaterials;
    for (const auto& m : materials) {
        auto mat = CreateRef<PBRMaterial>(m_context);
        
        if (!m.diffuse_texname.empty()) {
            std::string texPath = baseDir + m.diffuse_texname;
            try {
                mat->setAlbedoMap(m_resourceManager.load<Texture>(texPath, true));
            } catch (...) { BB_CORE_WARN("Model: Failed to load OBJ texture {}", texPath); }
        }
        
        PBRParameters params;
        params.baseColorFactor = glm::vec4(m.diffuse[0], m.diffuse[1], m.diffuse[2], 1.0f);
        params.metallicFactor = m.metallic;
        params.roughnessFactor = m.roughness;
        mat->setParameters(params);
        modelMaterials.push_back(mat);
    }

    // 2. Process geometries
    for (const auto& shape : shapes) {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::unordered_map<Vertex, uint32_t> uniqueVertices{};

        // If recalculating normals, we do it per triangle
        size_t indexOffset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
            size_t fv = size_t(shape.mesh.num_face_vertices[f]);
            if (fv != 3) {
                indexOffset += fv;
                continue; // Only support triangles
            }

            // Get vertices for this triangle
            glm::vec3 v[3];
            tinyobj::index_t idx[3];
            for (size_t v_i = 0; v_i < 3; v_i++) {
                idx[v_i] = shape.mesh.indices[indexOffset + v_i];
                v[v_i] = { attrib.vertices[3 * idx[v_i].vertex_index + 0], 
                           attrib.vertices[3 * idx[v_i].vertex_index + 1], 
                           attrib.vertices[3 * idx[v_i].vertex_index + 2] };
            }

            // Calculate flat normal for the triangle
            glm::vec3 flatNormal = glm::normalize(glm::cross(v[1] - v[0], v[2] - v[0]));

            for (size_t v_i = 0; v_i < 3; v_i++) {
                Vertex vertex{};
                const auto& index = idx[v_i];
                vertex.position = v[v_i];
                
                if (index.texcoord_index >= 0) {
                    vertex.uv = { attrib.texcoords[2 * index.texcoord_index + 0], attrib.texcoords[2 * index.texcoord_index + 1] };
                }
                
                if (config.recalculateNormals) {
                    vertex.normal = flatNormal;
                } else if (index.normal_index >= 0) {
                    vertex.normal = { attrib.normals[3 * index.normal_index + 0], attrib.normals[3 * index.normal_index + 1], attrib.normals[3 * index.normal_index + 2] };
                } else {
                    vertex.normal = flatNormal;
                }
                
                vertex.tangent = {1.0f, 0.0f, 0.0f, 1.0f};
                vertex.color = {1.0f, 1.0f, 1.0f}; 

                // Don't merge vertices if we need unique flat shading per face, otherwise it might average normals wrong or share vertices improperly across sharp edges
                // Actually, unordered_map relies on the normal to be part of the hash, so it naturally splits sharp edges!
                if (uniqueVertices.count(vertex) == 0) {
                    uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(vertex);
                }
                indices.push_back(uniqueVertices[vertex]);
            }
            indexOffset += fv;
        }

        auto mesh = CreateRef<Mesh>(m_context, vertices, indices);
        
        if (!shape.mesh.material_ids.empty() && shape.mesh.material_ids[0] >= 0) {
            size_t matId = static_cast<size_t>(shape.mesh.material_ids[0]);
            if (matId < modelMaterials.size()) {
                mesh->setMaterial(modelMaterials[matId]);
            }
        }

        m_bounds.extend(mesh->getBounds());
        m_meshes.push_back(std::move(mesh));
    }
    BB_CORE_INFO("Model: Successfully loaded OBJ '{0}' with {1} meshes", path, m_meshes.size());
}

void Model::loadGLTF(std::string_view path, const ModelLoadConfig& config) {
    auto absPath = std::filesystem::absolute(path);
    BB_CORE_INFO("Model: Loading GLTF {} (Preset: {})", absPath.string(), (int)config.preset);

    fastgltf::Parser parser;
    constexpr auto options = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages;

    auto data = fastgltf::GltfDataBuffer::FromPath(absPath);
    if (data.error() != fastgltf::Error::None) {
        BB_CORE_ERROR("GLTF: Failed to load file {0} (Error: {1})", absPath.string(), (int)data.error());
        throw std::runtime_error("GLTF: Failed to load file");
    }

    auto type = fastgltf::determineGltfFileType(data.get());
    auto assetParse = (type == fastgltf::GltfType::GLB) ? parser.loadGltfBinary(data.get(), absPath.parent_path(), options) : parser.loadGltf(data.get(), absPath.parent_path(), options);
    
    if (assetParse.error() != fastgltf::Error::None) {
        BB_CORE_ERROR("GLTF: Parse error in {0} (Error: {1})", absPath.string(), (int)assetParse.error());
        throw std::runtime_error("GLTF: Parse error");
    }

    const auto& gltf = assetParse.get();

    // 1. Process Textures
    for (const auto& image : gltf.images) {
        auto texture = std::visit(fastgltf::visitor {
            [&](const fastgltf::sources::Vector& vector) -> Ref<Texture> { return CreateRef<Texture>(m_context, std::span<const std::byte>(vector.bytes.data(), vector.bytes.size())); },
            [&](const fastgltf::sources::Array& array) -> Ref<Texture> { return CreateRef<Texture>(m_context, std::span<const std::byte>(array.bytes.data(), array.bytes.size())); },
            [&](const fastgltf::sources::ByteView& byteView) -> Ref<Texture> { return CreateRef<Texture>(m_context, byteView.bytes); },
            [&](const fastgltf::sources::BufferView& view) -> Ref<Texture> {
                auto& bv = gltf.bufferViews[view.bufferViewIndex];
                const std::byte* ptr = getBufferData(gltf.buffers[bv.bufferIndex]);
                return ptr ? CreateRef<Texture>(m_context, std::span<const std::byte>(ptr + bv.byteOffset, bv.byteLength)) : nullptr;
            },
            [&](const fastgltf::sources::URI& uri) -> Ref<Texture> {
                if (uri.uri.isLocalPath()) {
                    auto imagePath = absPath.parent_path() / uri.uri.path();
                    try { return m_resourceManager.load<Texture>(imagePath.string(), true); }
                    catch (...) { BB_CORE_ERROR("Model: GLTF failed to load external texture: {}", imagePath.string()); }
                }
                return nullptr;
            },
            [](auto&) -> Ref<Texture> { return nullptr; }
        }, image.data);
        m_textures.push_back(texture);
    }

    // 2. Load Materials
    std::vector<Ref<Material>> materials;
    for (const auto& mat : gltf.materials) {
        Ref<Material> material;
        bool isUnlit = mat.unlit;

        if (isUnlit) {
            auto unlit = CreateRef<UnlitMaterial>(m_context);
            unlit->setColor(glm::make_vec4(mat.pbrData.baseColorFactor.data()));
            if (mat.pbrData.baseColorTexture.has_value()) {
                size_t texIdx = mat.pbrData.baseColorTexture->textureIndex;
                if (gltf.textures[texIdx].imageIndex.has_value()) {
                    size_t imgIdx = gltf.textures[texIdx].imageIndex.value();
                    if (imgIdx < m_textures.size()) unlit->setBaseMap(m_textures[imgIdx]);
                }
            }
            material = unlit;
        } else if (config.preset == ModelLoadPreset::CellShading) {
            auto toon = CreateRef<ToonMaterial>(m_context);
            ToonParameters params;
            params.baseColor = glm::vec4(mat.pbrData.baseColorFactor[0], mat.pbrData.baseColorFactor[1], mat.pbrData.baseColorFactor[2], mat.pbrData.baseColorFactor[3]);
            params.emissiveFactor = glm::make_vec3(mat.emissiveFactor.data());
            
            if (mat.pbrData.baseColorTexture.has_value()) {
                size_t texIdx = mat.pbrData.baseColorTexture->textureIndex;
                if (gltf.textures[texIdx].imageIndex.has_value()) {
                    size_t imgIdx = gltf.textures[texIdx].imageIndex.value();
                    if (imgIdx < m_textures.size()) toon->setBaseMap(m_textures[imgIdx]);
                }
            }
            
            if (config.loadAlphaModes) {
                params.alphaMode = mat.alphaMode == fastgltf::AlphaMode::Mask ? AlphaMode::Mask : (mat.alphaMode == fastgltf::AlphaMode::Blend ? AlphaMode::Blend : AlphaMode::Opaque);
                params.alphaCutoff = mat.alphaCutoff;
                params.doubleSided = mat.doubleSided;
            }
            material = toon;
        } else {
            auto pbr = CreateRef<PBRMaterial>(m_context);
            PBRParameters params;
            params.baseColorFactor = glm::make_vec4(mat.pbrData.baseColorFactor.data());
            params.emissiveFactor = glm::make_vec3(mat.emissiveFactor.data());
            params.metallicFactor = mat.pbrData.metallicFactor;
            params.roughnessFactor = mat.pbrData.roughnessFactor;
            params.alphaCutoff = mat.alphaCutoff;
            
            BB_CORE_INFO("Model: Material '{0}' - BaseColor:({1},{2},{3},{4}), Metallic:{5}, Roughness:{6}", 
                mat.name, params.baseColorFactor.r, params.baseColorFactor.g, params.baseColorFactor.b, params.baseColorFactor.a,
                params.metallicFactor, params.roughnessFactor);
            
            auto getTex = [&](const auto& info) -> Ref<Texture> {
                if (info.has_value() && gltf.textures[info->textureIndex].imageIndex.has_value()) {
                    size_t imgIdx = gltf.textures[info->textureIndex].imageIndex.value();
                    return imgIdx < m_textures.size() ? m_textures[imgIdx] : nullptr;
                }
                return nullptr;
            };

            pbr->setAlbedoMap(getTex(mat.pbrData.baseColorTexture));
            if (config.loadPBRMaps) {
                pbr->setNormalMap(getTex(mat.normalTexture));
                pbr->setORMMap(getTex(mat.pbrData.metallicRoughnessTexture));
                pbr->setEmissiveMap(getTex(mat.emissiveTexture));
                if (mat.normalTexture.has_value()) params.normalScale = (float)mat.normalTexture->scale;
            }

            if (config.loadAlphaModes) {
                params.alphaMode = mat.alphaMode == fastgltf::AlphaMode::Mask ? AlphaMode::Mask : (mat.alphaMode == fastgltf::AlphaMode::Blend ? AlphaMode::Blend : AlphaMode::Opaque);
                params.alphaCutoff = mat.alphaCutoff;
                params.doubleSided = mat.doubleSided;
            }
            pbr->setParameters(params);
            material = pbr;
        }
        materials.push_back(material);
    }

    // 3. Recursive Node Processing
    auto processNode = [&](auto self, size_t nodeIdx, const glm::mat4& parentTransform) -> void {
        const auto& node = gltf.nodes[nodeIdx];
        glm::mat4 localTransform(1.0f);
        
        if (auto* matrix = std::get_if<fastgltf::math::fmat4x4>(&node.transform)) {
            localTransform = glm::make_mat4(matrix->data());
        } else if (auto* trs = std::get_if<fastgltf::TRS>(&node.transform)) {
            localTransform = glm::translate(glm::mat4(1.0f), glm::make_vec3(trs->translation.data())) *
                             glm::mat4_cast(glm::quat(trs->rotation[3], trs->rotation[0], trs->rotation[1], trs->rotation[2])) *
                             glm::scale(glm::mat4(1.0f), glm::make_vec3(trs->scale.data()));
        }
        
        glm::mat4 worldTransform = parentTransform * localTransform;

        if (node.meshIndex.has_value()) {
            const auto& mesh = gltf.meshes[node.meshIndex.value()];
            for (const auto& prim : mesh.primitives) {
                std::vector<Vertex> vertices;
                std::vector<uint32_t> indices;

                // Load Indices
                if (prim.indicesAccessor.has_value()) {
                    auto& acc = gltf.accessors[prim.indicesAccessor.value()];
                    indices.resize(acc.count);
                    fastgltf::iterateAccessorWithIndex<uint32_t>(gltf, acc, [&](uint32_t idx, size_t i) { indices[i] = idx; });
                }

                // Load Attributes
                const auto* posAttr = prim.findAttribute("POSITION");
                if (posAttr != prim.attributes.end()) {
                    auto& acc = gltf.accessors[posAttr->accessorIndex];
                    vertices.resize(acc.count);
                    fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, acc, [&](glm::vec3 pos, size_t i) {
                        vertices[i].position = glm::vec3(worldTransform * glm::vec4(pos, 1.0f));
                        vertices[i].normal = glm::normalize(glm::mat3(glm::transpose(glm::inverse(worldTransform))) * glm::vec3(0,1,0)); // Default normal
                        vertices[i].uv = {0.0f, 0.0f};
                        vertices[i].color = {1.0f, 1.0f, 1.0f}; // Default white
                        vertices[i].tangent = {1.0f, 0.0f, 0.0f, 1.0f};
                    });
                }

                if (const auto* nIdx = prim.findAttribute("NORMAL"); nIdx != prim.attributes.end()) {
                    fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[nIdx->accessorIndex], [&](glm::vec3 n, size_t i) {
                        vertices[i].normal = glm::normalize(glm::mat3(glm::transpose(glm::inverse(worldTransform))) * n);
                    });
                }

                if (const auto* uvIdx = prim.findAttribute("TEXCOORD_0"); uvIdx != prim.attributes.end()) {
                    fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[uvIdx->accessorIndex], [&](glm::vec2 uv, size_t i) { vertices[i].uv = uv; });
                }

                if (config.loadVertexColors) {
                    if (const auto* colIdx = prim.findAttribute("COLOR_0"); colIdx != prim.attributes.end()) {
                        fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[colIdx->accessorIndex], [&](glm::vec3 col, size_t i) { vertices[i].color = col; });
                    }
                }

                if (config.loadAnimations) {
                    if (const auto* jIdx = prim.findAttribute("JOINTS_0"); jIdx != prim.attributes.end()) {
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::uvec4>(gltf, gltf.accessors[jIdx->accessorIndex], [&](fastgltf::math::uvec4 j, size_t i) { 
                            vertices[i].joints = glm::ivec4(j[0], j[1], j[2], j[3]); 
                        });
                    }
                    if (const auto* wIdx = prim.findAttribute("WEIGHTS_0"); wIdx != prim.attributes.end()) {
                        fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[wIdx->accessorIndex], [&](glm::vec4 w, size_t i) { vertices[i].weights = w; });
                    }
                }

                auto newMesh = CreateRef<Mesh>(m_context, vertices, indices);
                if (prim.materialIndex.has_value() && config.loadMaterials) {
                    newMesh->setMaterial(materials[prim.materialIndex.value()]);
                }
                
                m_bounds.extend(newMesh->getBounds());
                m_meshes.push_back(std::move(newMesh));
            }
        }

        for (size_t childIdx : node.children) self(self, childIdx, worldTransform);
    };

    if (gltf.defaultScene.has_value()) {
        const auto& scene = gltf.scenes[gltf.defaultScene.value()];
        for (size_t nodeIdx : scene.nodeIndices) processNode(processNode, nodeIdx, glm::scale(glm::mat4(1.0f), config.initialScale));
    }

    BB_CORE_INFO("Model: Successfully loaded GLTF '{}' with {} meshes", absPath.string(), m_meshes.size());
}

} // namespace bb3d