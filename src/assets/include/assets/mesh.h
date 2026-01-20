/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <core/vertex.h>

#include <memory>
#include <vector>

namespace assets
{
struct Material;

struct SubMesh
{
    std::vector<core::Vertex> vertices;
    std::vector<uint32_t> indices;
    Material* material{nullptr};
};

struct Mesh
{
    std::vector<std::unique_ptr<SubMesh>> subMeshes;
};
} // namespace assets
