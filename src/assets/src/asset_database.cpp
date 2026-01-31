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

uint32_t AssetDatabase::vertexCount() const
{
    auto count = size_t{0};
    for (const auto& prefabItr : prefabs_)
    {
        count += prefabItr.second->vertexCount();
    }

    return static_cast<uint32_t>(count);
}

uint32_t AssetDatabase::indexCount() const
{
    auto count = size_t{0};
    for (const auto& prefabItr : prefabs_)
    {
        count += prefabItr.second->indexCount();
    }

    return static_cast<uint32_t>(count);
}

uint32_t AssetDatabase::materialCount() const
{
    auto count = size_t{0};
    for (const auto& prefabItr : prefabs_)
    {
        count += prefabItr.second->materials().size();
    }

    return static_cast<uint32_t>(count);
}

uint32_t AssetDatabase::imageCount() const
{
    auto count = size_t{0};
    for (const auto& prefabItr : prefabs_)
    {
        count += prefabItr.second->images().size();
    }

    return static_cast<uint32_t>(count);
}

uint32_t AssetDatabase::skyboxCount() const
{
    return static_cast<uint32_t>(skyboxes_.size());
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
