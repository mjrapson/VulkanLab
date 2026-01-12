/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "prefab.h"

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

    const AssetStorage<Prefab>& prefabs() const;

    void clear();

  private:
    AssetStorage<Prefab> prefabs_;
};
} // namespace assets
