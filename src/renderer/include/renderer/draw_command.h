/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <glm/glm.hpp>

namespace assets
{
struct Mesh;
} // namespace assets

namespace renderer
{
struct DrawCommand
{
    assets::Mesh* mesh;
    glm::mat4 transform;
};
} // namespace renderer
