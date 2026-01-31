/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "prefab.h"
#include "skybox.h"

#include <memory>
#include <unordered_map>

namespace assets
{
class AssetDatabase
{
  public:
    template <typename AssetType>
    using AssetStorage = std::unordered_map<std::string, std::unique_ptr<AssetType>>;

    void addPrefab(const std::string& name, std::unique_ptr<Prefab> prefab);
    void addSkybox(const std::string& name, std::unique_ptr<Skybox> skybox);

    uint32_t vertexCount() const;
    uint32_t indexCount() const;
    uint32_t materialCount() const;
    uint32_t imageCount() const;
    uint32_t skyboxCount() const;

    const AssetStorage<Prefab>& prefabs() const;
    const AssetStorage<Skybox>& skyboxes() const;

    void clear();

  private:
    AssetStorage<Prefab> prefabs_;
    AssetStorage<Skybox> skyboxes_;
};
} // namespace assets
