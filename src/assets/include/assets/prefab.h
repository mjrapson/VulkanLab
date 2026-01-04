/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "asset_handle.h"
#include "mesh.h"

#include <vector>

namespace assets
{
class Prefab
{
  public:
    Prefab(const std::vector<AssetHandle<Mesh>>&& meshes);

    const std::vector<AssetHandle<Mesh>>& meshes() const;

  private:
    std::vector<AssetHandle<Mesh>> meshes_;
};
} // namespace assets
