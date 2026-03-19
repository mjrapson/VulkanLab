/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <stdint.h>

namespace core
{
template <typename Object>
struct Handle
{
    uint32_t index;
    uint32_t generation;
};
} // namespace core
