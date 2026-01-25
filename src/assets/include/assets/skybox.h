/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <memory>

namespace assets
{
struct Image;

struct Skybox
{
    std::array<std::unique_ptr<Image>, 6> images;
};
} // namespace assets
