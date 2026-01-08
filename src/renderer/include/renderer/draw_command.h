/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <assets/asset_handle.h>

#include <glm/glm.hpp>

namespace assets
{
struct Material;
struct Mesh;
} // namespace assets

namespace renderer
{
struct DrawCommand
{
    assets::AssetHandle<assets::Material> material;
    assets::AssetHandle<assets::Mesh> mesh;
    glm::mat4 transform;
};
} // namespace renderer
