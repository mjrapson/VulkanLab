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

    constexpr bool operator==(const AssetHandle<AssetType>& rhs) const
    {
        return index == rhs.index;
    }
};
} // namespace assets

namespace std
{
template <typename AssetType>
struct hash<assets::AssetHandle<AssetType>>
{
    size_t operator()(const assets::AssetHandle<AssetType>& handle) const noexcept
    {
        return std::hash<uint32_t>{}(handle.index);
    }
};
} // namespace std
