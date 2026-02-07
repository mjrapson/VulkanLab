/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "image.h"
#include "material.h"
#include "mesh.h"
#include "mesh_instance.h"

#include <memory>
#include <ranges>
#include <unordered_map>
#include <vector>

namespace assets
{
class Prefab
{
    template <typename Handle, typename Resource>
    using Container = std::unordered_map<Handle, Resource, core::Hash<Handle>>;

  public:
    void addMaterial(std::unique_ptr<Material> material);
    void addMesh(std::unique_ptr<Mesh> mesh);
    void addImage(std::unique_ptr<Image> image);
    void addMeshInstance(MeshInstance&& instance);

    uint32_t vertexCount() const;
    uint32_t indexCount() const;

    Mesh* mesh(MeshHandle handle) const;

    auto materials() const
    {
        return materials_
               | std::views::transform(
                   [](const auto& ptr) -> const Material&
                   {
                       return *ptr.second;
                   });
    }

    auto meshes() const
    {
        return meshes_
               | std::views::transform(
                   [](const auto& ptr) -> const Mesh&
                   {
                       return *ptr.second;
                   });
    }

    auto images() const
    {
        return images_
               | std::views::transform(
                   [](const auto& ptr) -> const Image&
                   {
                       return *ptr.second;
                   });
    }

    auto meshInstances() const
    {
        return std::views::all(meshInstances_);
    }

  private:
    Container<MaterialHandle, std::unique_ptr<Material>> materials_;
    Container<MeshHandle, std::unique_ptr<Mesh>> meshes_;
    Container<ImageHandle, std::unique_ptr<Image>> images_;
    std::vector<MeshInstance> meshInstances_;
};
} // namespace assets
