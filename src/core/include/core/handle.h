/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <functional>
#include <stddef.h>
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

template <typename HandleType>
struct Hash
{
    size_t operator()(const core::Handle<HandleType>& handle) const noexcept
    {
        return std::hash<uint32_t>{}(handle.uid);
    }
};
} // namespace core
