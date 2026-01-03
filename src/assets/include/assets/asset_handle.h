/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <cstddef>
#include <functional>
#include <stdint.h>

namespace assets
{
template <typename AssetType>
struct AssetHandle
{
    uint32_t index;

    bool operator==(const AssetHandle<AssetType>& rhs) const
    {
        return index == rhs.index;
    }
};

template <typename AssetType>
struct AssetHandleHash
{
    std::size_t operator()(const AssetHandle<AssetType>& handle) const
    {
        return std::hash<uint32_t>{}(handle.index);
    }
};
} // namespace assets
