/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "asset_handle.h"

#include <optional>
#include <stdint.h>
#include <unordered_map>

namespace assets
{
template <typename AssetType>
class AssetStorage
{
  public:
    AssetHandle<AssetType> add(AssetType&& asset)
    {
        auto handle = AssetHandle<AssetType>{nextIndex_++};
        store_.emplace(handle, std::move(asset));

        return handle;
    }

    std::optional<std::reference_wrapper<const AssetType>>
    get(const AssetHandle<AssetType>& handle) const
    {
        auto itr = store_.find(handle);
        if (itr == store_.end())
        {
            return std::nullopt;
        }

        return itr->second;
    }

    void remove(const AssetHandle<AssetType>& handle)
    {
        store_.erase(handle);
    }

    void clear()
    {
        store_.clear();
    }

  private:
    std::unordered_map<AssetHandle<AssetType>, AssetType, AssetHandleHash<AssetType>> store_;
    uint32_t nextIndex_{0};
};
} // namespace assets
