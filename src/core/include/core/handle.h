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
