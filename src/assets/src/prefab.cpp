/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "assets/prefab.h"

namespace assets
{
Prefab::Prefab(const std::vector<AssetHandle<Mesh>>&& meshes)
    : meshes_{std::move(meshes)}
{
}

const std::vector<AssetHandle<Mesh>>& Prefab::meshes() const
{
    return meshes_;
}
} // namespace assets
