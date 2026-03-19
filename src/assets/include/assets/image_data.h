/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <stdint.h>
#include <vector>

namespace assets
{
struct ImageData
{
    uint32_t width;
    uint32_t height;
    uint32_t components;
    std::vector<std::byte> data;
};
} // namespace assets
