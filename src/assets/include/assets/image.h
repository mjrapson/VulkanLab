/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <stdint.h>
#include <vector>

namespace assets
{
struct Image
{
    uint32_t width;
    uint32_t height;
    std::vector<std::byte> data;
};
} // namespace assets
