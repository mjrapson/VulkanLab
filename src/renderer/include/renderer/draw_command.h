/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <assets/asset_handle.h>
#include <assets/mesh.h>

#include <glm/glm.hpp>

namespace renderer
{
struct DrawCommand
{
    assets::AssetHandle<assets::Mesh> mesh;
    glm::mat4 transform;
};
} // namespace renderer
