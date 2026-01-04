/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "asset_handle.h"
#include "asset_storage.h"
#include "image.h"
#include "material.h"
#include "mesh.h"
#include "prefab.h"

#include <filesystem>
#include <optional>
#include <vector>

namespace assets
{
class AssetDatabase
{
  public:
    AssetHandle<Image> addImage(Image&& image);
    AssetHandle<Material> addMaterial(Material&& material);
    AssetHandle<Mesh> addMesh(Mesh&& mesh);
    AssetHandle<Prefab> addPrefab(Prefab&& prefab);

    std::optional<std::reference_wrapper<const Image>>
    getImage(const AssetHandle<Image>& handle) const;

    std::optional<std::reference_wrapper<const Material>>
    getMaterial(const AssetHandle<Material>& handle) const;

    std::optional<std::reference_wrapper<const Mesh>>
    getMesh(const AssetHandle<Mesh>& handle) const;

    std::optional<std::reference_wrapper<const Prefab>>
    getPrefab(const AssetHandle<Prefab>& handle) const;

    void clear();

  private:
    AssetStorage<Image> images_;
    AssetStorage<Material> materials_;
    AssetStorage<Mesh> meshes_;
    AssetStorage<Prefab> prefabs_;
};
} // namespace assets
