// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "assets/asset_handle.h"
#include "assets/image.h"
#include "assets/material.h"
#include "assets/mesh.h"

#include <filesystem>
#include <unordered_map>

namespace tinygltf
{
class Model;
struct Primitive;
} // namespace tinygltf

namespace assets
{
class AssetDatabase;

class GltfLoader
{
  public:
    explicit GltfLoader(AssetDatabase& db);

    ~GltfLoader() = default;

    GltfLoader(const GltfLoader&) = delete;
    GltfLoader& operator=(const GltfLoader&) = delete;

    GltfLoader(GltfLoader&& other) = delete;
    GltfLoader& operator=(GltfLoader&& other) = delete;

    bool load(const std::filesystem::path& path);

  private:
    AssetHandle<Mesh> readMeshPrimitive(tinygltf::Primitive& primitive, tinygltf::Model& modell);
    std::optional<AssetHandle<Material>> readMaterial(int index, tinygltf::Model& model);
    std::optional<AssetHandle<Image>> readTextureImage(int index, tinygltf::Model& model);

  private:
    AssetDatabase& db_;
    std::unordered_map<int, AssetHandle<Image>> imageCache_;
    std::unordered_map<int, AssetHandle<Material>> materialCache_;
};
} // namespace assets
