// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "gltf_loader.h"

#include "image_loader.h"

#include <assets/image.h>
#include <assets/image_data.h>
#include <assets/material.h>
#include <assets/mesh.h>
#include <assets/prefab.h>

#include <core/vertex.h>

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

#include <spdlog/spdlog.h>

#include <memory>
#include <vector>

glm::vec3 readColor(const std::vector<double>& color)
{
    if (color.empty() || color.size() < 3)
    {
        return glm::vec3{1.0f, 1.0f, 1.0f};
    }

    return glm::vec3(color.at(0), color.at(1), color.at(2));
}

std::vector<uint32_t> readIndices(tinygltf::Primitive& primitive, tinygltf::Model& model)
{
    const auto& indexAcessor = model.accessors[primitive.indices];
    const auto& indexBufferView = model.bufferViews[indexAcessor.bufferView];
    const auto& indexBuffer = model.buffers[indexBufferView.buffer];

    auto indices = std::vector<uint32_t>{};

    if (indexAcessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
    {
        const auto* indexData = reinterpret_cast<const uint16_t*>(
            &indexBuffer.data[indexBufferView.byteOffset + indexAcessor.byteOffset]);
        for (auto i = size_t{0}; i < indexAcessor.count; ++i)
        {
            indices.push_back(static_cast<uint32_t>(indexData[i]));
        }
    }
    else
    {
        const auto* indexData = reinterpret_cast<const uint32_t*>(
            &indexBuffer.data[indexBufferView.byteOffset + indexAcessor.byteOffset]);
        for (auto i = size_t{0}; i < indexAcessor.count; ++i)
        {
            indices.push_back(static_cast<uint32_t>(indexData[i]));
        }
    }

    return indices;
}

std::vector<core::Vertex> readVertices(tinygltf::Primitive& primitive, tinygltf::Model& model)
{
    const auto& posAcessor = model.accessors[primitive.attributes.at("POSITION")];
    const auto& posBufferView = model.bufferViews[posAcessor.bufferView];
    const auto& posBuffer = model.buffers[posBufferView.buffer];

    const float* positions = reinterpret_cast<const float*>(
        &posBuffer.data[posBufferView.byteOffset + posAcessor.byteOffset]);

    const auto& normalAcessor = model.accessors[primitive.attributes.at("NORMAL")];
    const auto& normalBufferView = model.bufferViews[normalAcessor.bufferView];
    const auto& normalBuffer = model.buffers[normalBufferView.buffer];

    const float* normals = reinterpret_cast<const float*>(
        &normalBuffer.data[normalBufferView.byteOffset + normalAcessor.byteOffset]);

    // const auto& texAcessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
    // const auto& texBufferView = model.bufferViews[texAcessor.bufferView];
    // const auto& texBuffer = model.buffers[texBufferView.buffer];

    // const float* texcoords = reinterpret_cast<const float*>(
    //     &texBuffer.data[texBufferView.byteOffset + texAcessor.byteOffset]);

    auto vertices = std::vector<core::Vertex>{};
    for (auto i = size_t{0}; i < posAcessor.count; ++i)
    {
        auto v = core::Vertex{};
        v.position = glm::vec3(positions[i * 3 + 0], positions[i * 3 + 1], positions[i * 3 + 2]);
        v.normal = glm::vec3(normals[i * 3 + 0], normals[i * 3 + 1], normals[i * 3 + 2]);
        // v.textureUV = glm::vec2(texcoords[i * 2 + 0], texcoords[i * 2 + 1]);
        vertices.push_back(v);
    }

    return vertices;
}

void parseNode(int index,
               tinygltf::Model& model,
               const glm::mat4& parentTransform,
               assets::Prefab& prefab,
               const std::vector<std::vector<assets::Mesh*>>& meshCache)
{
    const auto& node = model.nodes[index];

    auto nodeTransform = glm::mat4{1.0f};

    if (!node.matrix.empty())
    {
        nodeTransform = glm::make_mat4(node.matrix.data());
    }
    else
    {
        if (!node.translation.empty())
        {
            nodeTransform = glm::translate(nodeTransform,
                                           glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
        }

        if (!node.rotation.empty())
        {
            nodeTransform *= glm::mat4_cast(glm::quat(static_cast<float>(node.rotation[3]),
                                                      static_cast<float>(node.rotation[0]),
                                                      static_cast<float>(node.rotation[1]),
                                                      static_cast<float>(node.rotation[2])));
        }

        if (!node.scale.empty())
        {
            nodeTransform = glm::scale(nodeTransform, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
        }
    }

    auto nodeToPrefab = parentTransform * nodeTransform;

    if (node.mesh >= 0)
    {
        auto meshInstance = assets::MeshInstance{};
        meshInstance.subMeshes = meshCache.at(node.mesh);
        meshInstance.transform = nodeToPrefab;
        prefab.meshInstances.push_back(std::move(meshInstance));
    }

    for (const auto& childIndex : node.children)
    {
        parseNode(childIndex, model, nodeToPrefab, prefab, meshCache);
    }
}

std::unique_ptr<assets::Prefab> loadGLTFModel(const std::filesystem::path& path)
{
    if (path.extension() != ".glb")
    {
        throw std::runtime_error("Unsupported gltf file: " + path.string());
    }

    auto model = tinygltf::Model{};
    auto loader = tinygltf::TinyGLTF{};
    auto err = std::string{};
    auto warn = std::string{};

    const auto ret = loader.LoadBinaryFromFile(&model, &err, &warn, path);

    if (!warn.empty())
    {
        spdlog::warn("{}", warn.c_str());
    }

    if (!err.empty())
    {
        spdlog::error("{}", err.c_str());
    }

    if (!ret)
    {
        spdlog::critical("Failed to parse glTF {}", path.string());
        return nullptr;
    }

    auto imageCache = std::unordered_map<std::string, assets::Image*>{};
    auto materialCache = std::unordered_map<std::string, assets::Material*>{};
    auto meshCache = std::vector<std::vector<assets::Mesh*>>{};

    auto prefab = std::make_unique<assets::Prefab>();

    for (const auto& tinyGltfImage : model.images)
    {
        auto image = std::make_unique<assets::Image>(
            createImageFromData(tinyGltfImage.width, tinyGltfImage.height, tinyGltfImage.image));

        imageCache.emplace(tinyGltfImage.name, image.get());
        prefab->images.push_back(std::move(image));
    }

    for (auto& gltfMaterial : model.materials)
    {
        auto material = std::make_unique<assets::Material>();

        material->setDiffuseColour(readColor(gltfMaterial.pbrMetallicRoughness.baseColorFactor));

        if (const auto index = gltfMaterial.pbrMetallicRoughness.baseColorTexture.index; index >= 0)
        {
            material->setDiffuseTexture(imageCache.at(model.images[model.textures[index].source].name));
        }

        materialCache.emplace(gltfMaterial.name, material.get());
        prefab->materials.push_back(std::move(material));
    }

    for (auto& gltfMesh : model.meshes)
    {
        auto subMeshCache = std::vector<assets::Mesh*>();
        for (auto& primitive : gltfMesh.primitives)
        {
            auto mesh = std::make_unique<assets::Mesh>();
            mesh->setVertices(readVertices(primitive, model));
            mesh->setIndices(readIndices(primitive, model));

            if (primitive.material >= 0)
            {
                mesh->setMaterial(materialCache.at(model.materials[primitive.material].name));
            }

            subMeshCache.push_back(mesh.get());
            prefab->meshes.push_back(std::move(mesh));
        }
        meshCache.push_back(subMeshCache);
    }

    auto& gltfScene = model.scenes[model.defaultScene];

    for (auto& nodeIndex : gltfScene.nodes)
    {
        parseNode(nodeIndex, model, glm::mat4{1.0f}, *prefab, meshCache);
    }

    return prefab;
}
