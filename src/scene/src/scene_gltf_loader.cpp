// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "scene/scene_gltf_loader.h"

#include "scene/entity.h"
#include "scene/scene.h"

#include <assets/image.h>
#include <assets/image_data.h>
#include <assets/loaders/image_loader.h>
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
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>

#include <spdlog/spdlog.h>

#include <memory>
#include <vector>

namespace scene
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

    auto vertices = std::vector<core::Vertex>{posAcessor.count};
    for (auto i = size_t{0}; i < vertices.size(); ++i)
    {
        vertices[i].position = glm::vec3(positions[i * 3 + 0], positions[i * 3 + 1], positions[i * 3 + 2]);
    }

    if (primitive.attributes.contains("NORMAL"))
    {
        const auto& normalAcessor = model.accessors[primitive.attributes.at("NORMAL")];
        const auto& normalBufferView = model.bufferViews[normalAcessor.bufferView];
        const auto& normalBuffer = model.buffers[normalBufferView.buffer];

        const float* normals = reinterpret_cast<const float*>(
            &normalBuffer.data[normalBufferView.byteOffset + normalAcessor.byteOffset]);

        for (auto i = size_t{0}; i < vertices.size(); ++i)
        {
            vertices[i].normal = glm::vec3(normals[i * 3 + 0], normals[i * 3 + 1], normals[i * 3 + 2]);
        }
    }
    // else consider generating surface normals?

    if (primitive.attributes.contains("TEXCOORD_0"))
    {
        const auto& texAcessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
        const auto& texBufferView = model.bufferViews[texAcessor.bufferView];
        const auto& texBuffer = model.buffers[texBufferView.buffer];

        const float* texcoords = reinterpret_cast<const float*>(
            &texBuffer.data[texBufferView.byteOffset + texAcessor.byteOffset]);

        for (auto i = size_t{0}; i < vertices.size(); ++i)
        {
            vertices[i].textureUV = glm::vec2(texcoords[i * 2 + 0], texcoords[i * 2 + 1]);
        }
    }
    // else consider generating UV from a surface projection?

    return vertices;
}

Entity parseNode(int index,
                 tinygltf::Model& model,
                 const glm::mat4& parentTransform,
                 const std::vector<core::Handle<assets::Prefab>>& prefabCache)
{
    auto entity = Entity{};

    const auto& node = model.nodes[index];

    spdlog::debug("Processing node {}", node.name);

    // Get transform, according to the spec, we have either a matrix (if not animated) or individual optional TRS
    // components - otherwise assume an identity transform matrix
    auto nodeTransform = glm::mat4{1.0f};
    if (!node.matrix.empty())
    {
        glm::mat4 mat = glm::make_mat4(node.matrix.data());
        nodeTransform *= mat;
    }
    else
    {
        // TRS order
        if (!node.translation.empty())
        {
            nodeTransform = glm::translate(nodeTransform,
                                           glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
        }

        if (!node.rotation.empty())
        {
            auto quat = glm::make_quat(node.rotation.data());
            glm::mat4 rotation = glm::mat4_cast(quat);
            nodeTransform *= rotation;
        }

        if (!node.scale.empty())
        {
            nodeTransform = glm::scale(nodeTransform, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
        }
    }

    // Child nodes local transforms are relative to their parent (global) transform
    entity.transform = parentTransform * nodeTransform;

    glm::vec3 scale;
    glm::quat rotation;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(entity.transform, scale, rotation, translation, skew, perspective);

    // Resolve renderable prefabs
    if (node.mesh >= 0)
    {
        entity.prefabHandle = prefabCache.at(node.mesh);
    }

    // Resolve camera
    if (node.camera >= 0)
    {
        const auto& gltfCamera = model.cameras[node.camera];
        if (gltfCamera.type == "perspective")
        {
            spdlog::debug("Adding perspective camera to node");

            auto camera = renderer::Camera{};
            camera.aspectRatio = static_cast<float>(gltfCamera.perspective.aspectRatio);
            camera.fieldOfView = static_cast<float>(glm::degrees(gltfCamera.perspective.yfov));
            camera.nearPlane = static_cast<float>(gltfCamera.perspective.znear);
            camera.farPlane = static_cast<float>(gltfCamera.perspective.zfar);

            camera.position = translation;
            camera.orientation = rotation * glm::angleAxis(glm::radians(180.0f), glm::vec3{0, 1, 0});

            entity.camera = camera;
        }
    }

    // Resolve lights
    if (node.light >= 0)
    {
        const auto& gltfLight = model.lights[node.light];
        spdlog::debug("Light type: {}", gltfLight.type);

        if (gltfLight.type == "directional")
        {
            spdlog::debug("Adding directional light to node");

            // Direction will come from the orientation of the node, +Z forward
            auto direction = rotation * glm::angleAxis(glm::radians(180.0f), glm::vec3{0, 1, 0}) * glm::vec3(0, 0, 1);

            auto light = DirectionalLight{};
            light.colour = readColor(gltfLight.color);
            light.direction = glm::normalize(direction);

            spdlog::debug("Light direction: {}", glm::to_string(light.direction));

            entity.directionalLight = light;
        }
    }

    // Resolve any children in the hierarchy
    for (const auto& childIndex : node.children)
    {
        entity.children.push_back(parseNode(childIndex, model, entity.transform, prefabCache));
    }

    return entity;
}

void loadGltfScene(const std::filesystem::path& path, Scene& scene)
{
    if (path.extension() != ".glb")
    {
        throw std::runtime_error("Unsupported gltf file: " + path.string());
    }

    auto model = tinygltf::Model{};
    auto loader = tinygltf::TinyGLTF{};
    auto err = std::string{};
    auto warn = std::string{};

    const auto ret = loader.LoadBinaryFromFile(&model, &err, &warn, path.string().c_str());

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
        return;
    }

    spdlog::debug("Importing {}", path.string());
    spdlog::debug("Found {} images, {} materials, {} meshes and {} nodes",
                  model.images.size(),
                  model.materials.size(),
                  model.meshes.size(),
                  model.nodes.size());

    auto imageCache = std::unordered_map<std::string, core::Handle<assets::Image>>{};
    auto materialCache = std::unordered_map<std::string, core::Handle<assets::Material>>{};
    auto prefabCache = std::vector<core::Handle<assets::Prefab>>{};

    for (const auto& tinyGltfImage : model.images)
    {
        spdlog::debug("Processing image {}", tinyGltfImage.name);
        auto imageData = assets::createImageFromData(tinyGltfImage.width, tinyGltfImage.height, tinyGltfImage.image);
        auto handle = scene.assetDatabase.createImage(std::move(imageData));
        imageCache.emplace(tinyGltfImage.name, handle);
    }

    for (auto& gltfMaterial : model.materials)
    {
        spdlog::debug("Processing material {}", gltfMaterial.name);
        auto handle = scene.assetDatabase.createMaterial();
        auto material = scene.assetDatabase.material(handle);
        materialCache.emplace(gltfMaterial.name, handle);

        material->setDiffuseColour(readColor(gltfMaterial.pbrMetallicRoughness.baseColorFactor));

        if (const int index = gltfMaterial.pbrMetallicRoughness.baseColorTexture.index; index >= 0)
        {
            material->setDiffuseTexture(imageCache.at(model.images[model.textures[index].source].name));
        }
    }

    for (auto& gltfMesh : model.meshes)
    {
        spdlog::debug("Processing mesh {}", gltfMesh.name);
        if (gltfMesh.primitives.empty())
        {
            continue;
        }

        auto prefabHandle = scene.assetDatabase.createPrefab();
        auto prefab = scene.assetDatabase.prefab(prefabHandle);
        prefabCache.push_back(prefabHandle);

        for (auto& primitive : gltfMesh.primitives)
        {
            auto meshHandle = scene.assetDatabase.createMesh();
            auto mesh = scene.assetDatabase.mesh(meshHandle);
            mesh->setVertices(readVertices(primitive, model));
            mesh->setIndices(readIndices(primitive, model));

            auto meshInstance = assets::MeshInstance{
                .meshHandle = meshHandle,
                .materialHandle = std::nullopt,
            };

            if (primitive.material >= 0)
            {
                meshInstance.materialHandle = materialCache.at(model.materials[primitive.material].name);
            }

            prefab->meshInstances.push_back(meshInstance);
        }
    }

    auto& gltfScene = model.scenes[model.defaultScene];
    for (auto& nodeIndex : gltfScene.nodes)
    {
        scene.entityGraph.appendEntity(parseNode(nodeIndex, model, glm::mat4{1.0f}, prefabCache));
    }
}
} // namespace scene
