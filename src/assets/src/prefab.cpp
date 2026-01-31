/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "assets/prefab.h"

namespace assets
{
void Prefab::addMaterial(std::unique_ptr<Material> material)
{
    if (!material)
    {
        return;
    }

    materials_[material->uid()] = std::move(material);
}

void Prefab::addMesh(std::unique_ptr<Mesh> mesh)
{
    if (!mesh)
    {
        return;
    }

    meshes_[mesh->uid()] = std::move(mesh);
}

void Prefab::addImage(std::unique_ptr<Image> image)
{
    if (!image)
    {
        return;
    }

    images_[image->uid()] = std::move(image);
}

void Prefab::addMeshInstance(MeshInstance&& instance)
{
    meshInstances_.push_back(std::move(instance));
}

uint32_t Prefab::vertexCount() const
{
    return static_cast<uint32_t>(std::ranges::fold_left(meshes_,
                                                        size_t{0},
                                                        [](size_t n, const auto& mesh)
                                                        {
                                                            return n + mesh.second->vertexCount();
                                                        }));
}

uint32_t Prefab::indexCount() const
{
    return static_cast<uint32_t>(std::ranges::fold_left(meshes_,
                                                        size_t{0},
                                                        [](size_t n, const auto& mesh)
                                                        {
                                                            return n + mesh.second->indexCount();
                                                        }));
}

Mesh* Prefab::mesh(uint32_t handle) const
{
    return meshes_.at(handle).get();
}
} // namespace assets
