/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <memory>

namespace assets
{
struct Image;

struct Skybox
{
    std::unique_ptr<Image> px;
    std::unique_ptr<Image> py;
    std::unique_ptr<Image> pz;
    std::unique_ptr<Image> nx;
    std::unique_ptr<Image> ny;
    std::unique_ptr<Image> nz;
};
} // namespace assets
