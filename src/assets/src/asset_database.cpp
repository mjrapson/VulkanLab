/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "assets/asset_database.h"

namespace assets
{
void AssetDatabase::addPrefab(const std::string& name, std::unique_ptr<Prefab> prefab)
{
    prefabs_[name] = std::move(prefab);
}

const AssetDatabase::AssetStorage<Prefab>& AssetDatabase::prefabs() const
{
    return prefabs_;
}

void AssetDatabase::clear()
{
    prefabs_.clear();
}
} // namespace assets
