/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <stdint.h>

namespace core
{
template <typename HandleType>
struct Handle
{
    uint32_t uid;

    bool operator==(const HandleType& other) const
    {
        return uid == other.uid;
    }
};
} // namespace core

namespace std
{
template <typename HandleType>
struct hash<core::Handle<HandleType>>
{
    std::size_t operator()(const core::Handle<HandleType>& handle) const noexcept
    {
        return std::hash<uint32_t>{}(handle.uid);
    }
};
} // namespace std
