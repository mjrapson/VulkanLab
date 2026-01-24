/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "assets/asset_database.h"

namespace assets
{
void AssetDatabase::addPrefab(const std::string& name, std::unique_ptr<Prefab> prefab)
{
    prefabs_[name] = std::move(prefab);
}

void AssetDatabase::addSkybox(const std::string& name, std::unique_ptr<Skybox> skybox)
{
    skyboxes_[name] = std::move(skybox);
}

const AssetDatabase::AssetStorage<Prefab>& AssetDatabase::prefabs() const
{
    return prefabs_;
}

const AssetDatabase::AssetStorage<Skybox>& AssetDatabase::skyboxes() const
{
    return skyboxes_;
}

void AssetDatabase::clear()
{
    prefabs_.clear();
    skyboxes_.clear();
}
} // namespace assets
