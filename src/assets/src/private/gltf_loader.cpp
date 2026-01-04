// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "gltf_loader.h"

#include "assets/asset_database.h"
#include "assets/image.h"
#include "assets/material.h"
#include "assets/mesh.h"
#include "assets/prefab.h"
#include "image_loader.h"

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

#include <spdlog/spdlog.h>

#include <vector>

namespace assets
{
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

    const auto& texAcessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
    const auto& texBufferView = model.bufferViews[texAcessor.bufferView];
    const auto& texBuffer = model.buffers[texBufferView.buffer];

    const float* texcoords = reinterpret_cast<const float*>(
        &texBuffer.data[texBufferView.byteOffset + texAcessor.byteOffset]);

    auto vertices = std::vector<core::Vertex>{};
    for (auto i = size_t{0}; i < posAcessor.count; ++i)
    {
        auto v = core::Vertex{};
        v.position = glm::vec3(positions[i * 3 + 0], positions[i * 3 + 1], positions[i * 3 + 2]);
        v.normal = glm::vec3(normals[i * 3 + 0], normals[i * 3 + 1], normals[i * 3 + 2]);
        v.textureUV = glm::vec2(texcoords[i * 2 + 0], 1.0f - texcoords[i * 2 + 1]);
        vertices.push_back(v);
    }

    return vertices;
}

GltfLoader::GltfLoader(AssetDatabase& db)
    : db_{db}
{
}

Prefab GltfLoader::load(const std::filesystem::path& path)
{
    if (path.extension() != ".glb")
    {
        throw std::runtime_error("Unsupported gtlf file: " + path.string());
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
        throw std::runtime_error("Failed to load gtlf file: " + path.string());
    }

    auto meshHandles = std::vector<AssetHandle<Mesh>>{};

    for (auto& gltfMesh : model.meshes)
    {
        for (auto& primitive : gltfMesh.primitives)
        {
            meshHandles.push_back(readMeshPrimitive(primitive, model));
        }
    }

    return Prefab{std::move(meshHandles)};
}

AssetHandle<Mesh> GltfLoader::readMeshPrimitive(tinygltf::Primitive& primitive,
                                                tinygltf::Model& model)
{
    auto mesh = Mesh{};
    mesh.vertices = readVertices(primitive, model);
    mesh.indices = readIndices(primitive, model);
    mesh.material = readMaterial(primitive.material, model);

    return db_.addMesh(std::move(mesh));
}

std::optional<AssetHandle<Material>> GltfLoader::readMaterial(int index, tinygltf::Model& model)
{
    if (index < 0)
    {
        return std::nullopt;
    }

    if (static_cast<size_t>(index) >= model.materials.size())
    {
        return std::nullopt;
    }

    if (auto itr = materialCache_.find(index); itr != materialCache_.end())
    {
        return itr->second;
    }

    auto& pbr = model.materials[index].pbrMetallicRoughness;

    auto material = Material{};
    material.diffuse = readColor(pbr.baseColorFactor);
    material.diffuseTexture = readTextureImage(pbr.baseColorTexture.index, model);

    const auto handle = db_.addMaterial(std::move(material));

    materialCache_[index] = handle;

    return handle;
}

std::optional<AssetHandle<Image>> GltfLoader::readTextureImage(int index, tinygltf::Model& model)
{
    if (index < 0)
    {
        return std::nullopt;
    }

    if (static_cast<size_t>(index) >= model.textures.size())
    {
        return std::nullopt;
    }

    if (auto itr = imageCache_.find(index); itr != imageCache_.end())
    {
        return itr->second;
    }

    const auto textureSource = model.textures[index].source;
    const auto imageWidth = model.images[textureSource].width;
    const auto imageHeight = model.images[textureSource].height;
    const auto& data = model.images[textureSource].image;

    const auto handle = db_.addImage(std::move(createImageFromData(imageWidth, imageHeight, data)));

    imageCache_[index] = handle;

    return handle;
}
} // namespace assets
