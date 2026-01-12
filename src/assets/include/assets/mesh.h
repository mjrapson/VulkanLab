/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <core/vertex.h>

#include <vector>

namespace assets
{
struct Material;

struct Mesh
{
    std::vector<core::Vertex> vertices;
    std::vector<uint32_t> indices;
    Material* material{nullptr};
};
} // namespace assets
