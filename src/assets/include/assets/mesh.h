/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "asset_handle.h"
#include "material.h"

#include <core/vertex.h>

#include <optional>
#include <vector>

namespace assets
{
struct Mesh
{
    std::vector<core::Vertex> vertices;
    std::vector<uint32_t> indices;
    std::optional<AssetHandle<Material>> material;
};
} // namespace assets
